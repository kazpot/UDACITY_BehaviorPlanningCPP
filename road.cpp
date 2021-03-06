#include <iostream>
#include "road.h"
#include "vehicle.h"
#include <cmath>
#include <map>
#include <iterator>


/**
 * Initializes Road
 */
Road::Road(int speed_limit, double traffic_density, std::vector<int> lane_speeds)
{
    this->num_lanes = lane_speeds.size();
    this->lane_speeds = lane_speeds;
    this->speed_limit = speed_limit;
    this->density = traffic_density;
    this->camera_center = this->update_width/2;
}

Road::~Road() {}

Vehicle Road::GetEgo()
{
    return this->vehicles.find(this->ego_key)->second;
}

void Road::PopulateTraffic()
{
    int start_s = std::max(this->camera_center - (this->update_width/2), 0);
    for (int l = 0; l < this->num_lanes; ++l)
    {
        int lane_speed = this->lane_speeds[l];
        bool vehicle_just_added = false;
        for (int s = start_s; s < start_s+this->update_width; ++s)
        {
            if (vehicle_just_added)
            {
                vehicle_just_added = false;
            }

            if (((double)rand() / (RAND_MAX)) < this->density)
            {
                
                Vehicle vehicle = Vehicle(l,s,lane_speed,0);
                vehicle.state_ = "CS";
                this->vehicles_added += 1;
                this->vehicles.insert(std::pair<int,Vehicle>(vehicles_added,vehicle));
                vehicle_just_added = true;
            }
        }
    }
}

void Road::Advance()
{
    std::map<int, std::vector<std::vector<int>>> predictions;
    auto it = this->vehicles.begin();
    while (it != this->vehicles.end())
    {
        int v_id = it->first;
        std::vector<std::vector<int>> preds = it->second.GeneratePredictions(10);
        predictions[v_id] = preds;
        ++it;
    }

    it = this->vehicles.begin();
    while (it != this->vehicles.end())
    {
        int v_id = it->first;
        if (v_id == ego_key)
        {
            it->second.UpdateState(predictions);
            it->second.RealizeState(predictions);
        }
        it->second.Increment(1);
        ++it;
    }
}

void Road::Display(int time_step)
{
    Vehicle ego = this->vehicles.find(this->ego_key)->second;
    int s = ego.s_;
    std::string state = ego.state_;

    this->camera_center = std::max(s, this->update_width / 2);
    int s_min = std::max(this->camera_center - this->update_width / 2, 0);
    int s_max = s_min + this->update_width;

    std::vector<std::vector<std::string>> road;

    for (int i = 0; i < this->update_width; ++i)
    {
        std::vector<std::string> road_lane;
        for (int ln = 0; ln < this->num_lanes; ++ln)
        {
            road_lane.push_back("     ");
        }
        road.push_back(road_lane);
    }

    auto it = this->vehicles.begin();
    while (it != this->vehicles.end())
    {

        int v_id = it->first;
        Vehicle v = it->second;

        if (s_min <= v.s_ && v.s_ < s_max)
        {
            std::string marker = "";
            if (v_id == this->ego_key)
            {
                marker = this->ego_rep;
            }
            else
            {
                
                std::stringstream oss;
                std::stringstream buffer;
                buffer << " ";
                oss << v_id;
                for (int buffer_i = oss.str().length(); buffer_i < 3; ++buffer_i)
                {
                    buffer << "0";
                
                }
                buffer << oss.str() << " ";
                marker = buffer.str();
            }
            road[int(v.s_ - s_min)][int(v.lane_)] = marker;
        }
        ++it;
    }
    std::ostringstream oss;
    oss << "+Meters ======================+ step: " << time_step << std::endl;
    int i = s_min;
    for (int lj = 0; lj < road.size(); ++lj)
    {
        if (i % 20 == 0)
        {
            std::stringstream buffer;
            std::stringstream dis;
            dis << i;
            for (int buffer_i = dis.str().length(); buffer_i < 3; ++buffer_i)
            {
                 buffer << "0";
            }
            
            oss << buffer.str() << dis.str() << " - ";
        }
        else
        {
            oss << "      ";
        }          
        ++i;

        for (int li = 0; li < road[0].size(); ++li)
        {
            oss << "|" << road[lj][li];
        }

        oss << "|";
        oss << "\n";
    }
    std::cout << oss.str();
}

void Road::AddEgo(int lane_num, int s, std::vector<int> config_data)
{
    auto it = this->vehicles.begin();
    while (it != this->vehicles.end())
    {
        int v_id = it->first;
        Vehicle v = it->second;
        if (v.lane_ == lane_num && v.s_ == s)
        {
            this->vehicles.erase(v_id);
        }
        ++it;
    }
    Vehicle ego = Vehicle(lane_num, s, this->lane_speeds[lane_num], 0);
    ego.Configure(config_data);
    ego.state_ = "KL";
    this->vehicles.insert(std::pair<int,Vehicle>(ego_key,ego));
    
}


