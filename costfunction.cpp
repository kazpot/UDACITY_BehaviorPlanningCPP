#include <iostream>
#include <cmath>
#include <map>
#include <iterator>
#include <algorithm>
#include "costfunction.h"

/**
 * Initializes Cost Function
 */
Costfunction::Costfunction() {}

Costfunction::~Costfunction() {}

double Costfunction::ChangeLaneCost(std::vector<Vehicle::Snapshot> trajectory, Costfunction::TrajectoryData &trajectory_data)
{
    // Penalizes lane changes AWAY from the goal lane and rewards
    // lane changes TOWARDS the goal lane.
    int proposed_lanes = trajectory_data.end_lanes_from_goal;
    int cur_lane = trajectory[0].lane;

    double cost = 0;
    if (proposed_lanes > cur_lane)
    {
        cost = COMFORT;
    }

    if (proposed_lanes < cur_lane){
        cost = -1.0 * COMFORT;
    }

    if (cost != 0){
        std::cout << "!!cost for lane change is " << cost << "\n\n";
    }

    return cost;
}

double Costfunction::DistanceFromGoalLane(Costfunction::TrajectoryData &trajectory_data)
{
    int distance = abs(trajectory_data.end_distance_to_goal);
    distance = std::max(distance, 1);
    int time_to_goal = (int)(distance / trajectory_data.avg_speed);

    int lanes = trajectory_data.end_lanes_from_goal;
    auto multiplier = (double)(5.0 * lanes / time_to_goal);
    double cost = multiplier * REACH_GOAL;
    return cost;
}

double Costfunction::InefficiencyCost(Vehicle &vehicle, Costfunction::TrajectoryData &trajectory_data)
{
    int speed = (int)trajectory_data.avg_speed;
    int target_speed = vehicle.target_speed_;
    int diff = target_speed - speed;
    double pct = double(diff) / target_speed;
    double multiplier = pow(pct, 2);
    return multiplier * EFFICIENCY;
}


double Costfunction::CollisionCost(Costfunction::TrajectoryData &trajectory_data)
{
    if (!trajectory_data.collides.empty())
    {
        int time_til_collision = trajectory_data.collides["at"];
        auto exponent = (double)pow(time_til_collision,2);
        double multiplier = exp(-1 * exponent);
        return multiplier * COLLISION;
    }
    return 0.0;
}

double Costfunction::BufferCost(Costfunction::TrajectoryData &trajectory_data)
{
    int closest = trajectory_data.closest_approach;
    if (closest == 0)
    {
        return 10 * DANGER;
    }

    int time_steps_away = (int)(closest / trajectory_data.avg_speed);
    if (time_steps_away > DESIRED_BUFFER)
    {
        return 0.0;
    }
    double multiplier = 1.0 - pow(time_steps_away / DESIRED_BUFFER, 2);
    return multiplier * DANGER;
}

double Costfunction::CalculateCost(Vehicle &vehicle, std::vector<Vehicle::Snapshot> trajectory, std::map<int,std::vector<std::vector<int>>> predictions, bool verbose=false)
{
    TrajectoryData trajectory_data = TrajectoryData();
    trajectory_data = this->GetHelperData(vehicle, trajectory, predictions);

    double cost = 0.0;
    cost += this->DistanceFromGoalLane(trajectory_data);
    cost += this->InefficiencyCost(vehicle, trajectory_data);
    cost += this->CollisionCost(trajectory_data);
    cost += this->BufferCost(trajectory_data);
    cost += this->ChangeLaneCost(trajectory, trajectory_data);

    return cost;
}

Costfunction::TrajectoryData Costfunction::GetHelperData(Vehicle &vehicle, std::vector<Vehicle::Snapshot> trajectory, std::map<int,std::vector<std::vector<int>>> &predictions)
{
    std::vector<Vehicle::Snapshot> t = trajectory;
    Vehicle::Snapshot current_snapshot = t.front();
    Vehicle::Snapshot first = t.at(1);
    Vehicle::Snapshot last = t.back();
    int end_distance_to_goal = vehicle.goal_s_ - last.s;
    int end_lanes_from_goal = abs(vehicle.goal_lane_ - last.lane);
    auto dt = (double)trajectory.size();
    int proposed_lane = first.lane;
    auto avg_speed = (double)((last.s - current_snapshot.s) / dt);

    //initialize variables
    std::map<std::string, int> collides;
    std::vector<int> accels;
    int closest_approach = 999999;

    std::map<int, std::vector<std::vector<int>>> filtered = this->FilterPredictionsByLane(predictions, proposed_lane);
    for (int i = 1; i < PLANNING_HORIZON + 1; ++i)
    {
        Vehicle::Snapshot ss = trajectory.at(i);

        int s = ss.s;
        int a = ss.a;
        accels.push_back(a);
        for (auto &item : filtered)
        {
            std::vector<std::vector<int>> v = item.second;
            std::vector<int> state = v.at(i);
            std::vector<int> last_state = v.at(i - 1);

            bool vehicle_collides = this->CheckCollision(ss, last_state.at(1), state.at(1));
            if (vehicle_collides)
            {
                collides["at"] = i;
            }

            int dist = abs(state.at(1) - s);
            if (dist < closest_approach)
            {
                closest_approach = dist;
            }
        }
    }
    int max_accel = *max_element(accels.begin(), accels.end());
    
    std::vector<int> rms_accels;
    for (int a : accels)
    {
        int rms = (int)pow(a, 2);
        rms_accels.push_back(rms);
    }

    double sum = (double)std::accumulate(rms_accels.begin(), rms_accels.end(), 0);
    int num_accels = rms_accels.size();
    auto rms_acceleration = (double)(sum / num_accels);

    TrajectoryData trajectory_data = TrajectoryData();
    trajectory_data.proposed_lane = proposed_lane;
    trajectory_data.avg_speed = avg_speed;
    trajectory_data.max_acceleration = max_accel;
    trajectory_data.rms_acceleration = rms_acceleration;
    trajectory_data.closest_approach = closest_approach;
    trajectory_data.end_distance_to_goal = end_distance_to_goal;
    trajectory_data.end_lanes_from_goal = end_lanes_from_goal;
    trajectory_data.collides = collides;

    return trajectory_data;
}

bool Costfunction::CheckCollision(Vehicle::Snapshot snapshot, double s_previous, double s_now)
{
    int s = snapshot.s;
    int v = snapshot.v;

    if (s_previous < s)
    {
        if (s_now >= s)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    
    if (s_previous > s)
    {
        if (s_now <= s)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    int v_target = (int)(s_now - s_previous);
    if (s_previous == s)
    {
        if (v_target > v)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    return false;
}

std::map<int, std::vector<std::vector<int>>> Costfunction::FilterPredictionsByLane(std::map<int, std::vector<std::vector<int>>> predictions, int lane)
{
    std::map<int, std::vector<std::vector<int>>> filtered;
    for (auto &map : predictions)
    {
        int v_id = map.first;
        if (v_id != -1)
        {
            filtered[v_id] = map.second;
        }
    }
    return filtered;
}