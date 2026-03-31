#pragma once
#ifndef ESVO2_CORE_ROS_FREE_TYPES_H
#define ESVO2_CORE_ROS_FREE_TYPES_H

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

using timePoint = std::chrono::time_point<std::chrono::steady_clock>;

namespace esvo2_core
{

// Replaces dvs_msgs::Event
struct Event
{
        uint16_t x;
        uint16_t y;
        timePoint timestamp;
        bool polarity;
};

// Replaces dvs_msgs::EventArray
struct EventArray
{
        std::vector<Event> events;
        uint32_t height;
        uint32_t width;
};

// Replaces sensor_msgs::Imu
struct ImuMsg
{
        timePoint timestamp;
        double linear_acceleration[3]; // x, y, z  (m/s^2)
        double angular_velocity[3];    // x, y, z  (rad/s)
};

// Replaces geometry_msgs::PoseStamped
struct PoseStamped
{
        timePoint timestamp;
        std::string frame_id;
        double position[3];    // x, y, z
        double orientation[4]; // x, y, z, w  (quaternion)
};

// Replaces nav_msgs::Path
struct Path
{
        timePoint header_stamp;
        std::string header_frame_id;
        std::vector<PoseStamped> poses;
};

// Replaces events_repacking_tool::V_ba_bg
struct VBaBg
{
        timePoint head;         // timestamp
        std::vector<double> ba; // bias acceleration
        std::vector<double> bg; // bias gyro
        std::vector<double> Vs; // velocity
        std::vector<double> g;  // gravity
};

} // namespace esvo2_core

#endif // ESVO2_CORE_ROS_FREE_TYPES_H
