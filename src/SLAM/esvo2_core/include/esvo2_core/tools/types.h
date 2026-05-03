#pragma once
#ifndef ESVO2_CORE_TYPES_H
#define ESVO2_CORE_TYPES_H

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <deque>
#include <iostream>

#include "SystemStatus.h"
#include <opencv2/core/core.hpp>
#include <glm/gtc/quaternion.hpp>
#include <kindr/minimal/quat-transformation.h>
#include <kindr/minimal/rotation-quaternion.h>



namespace esvo2_core
{
using timePoint = std::chrono::time_point<std::chrono::steady_clock>;

inline double timePointToSec(const timePoint& timestamp)
{
//      return std::chrono::duration<double>(timestamp.time_since_epoch()).count();
     return timestamp.time_since_epoch().count() / (1000000000.0);
}

inline timePoint secondsToTimePoint(double seconds)
{
        std::chrono::duration<double> dur(seconds);
        std::chrono::steady_clock::duration yep = std::chrono::duration_cast<std::chrono::steady_clock::duration>(dur);
        return std::chrono::time_point<std::chrono::steady_clock>(yep);
}

// Replaces Event
struct Event
{
        uint16_t x;
        uint16_t y;
        timePoint timestamp;
        bool polarity;
};

using EventQueue = std::deque<Event>;

// Replaces EventArray
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

// Replaces sensor_msgs::ImageConstPtr
struct ImagePtr
{
    timePoint header_stamp;
    std::shared_ptr<cv::Mat> image;
    public:
        ImagePtr() {}
        ImagePtr& operator=(const ImagePtr& other)
        {
                if(this == &other)
                        return *this;
                this->header_stamp = other.header_stamp;
                this->image = std::make_shared<cv::Mat>();
                *(this->image) = other.image->clone();
                return *this;
        }
        ImagePtr(std::shared_ptr<cv::Mat> image, std::chrono::time_point<std::chrono::steady_clock> time)
        {
            this->image = image;
            this->header_stamp = time;
        }
        ImagePtr(std::pair<std::shared_ptr<cv::Mat>, std::chrono::time_point<std::chrono::steady_clock>> stuff)
        {
            this->image = stuff.first;
            this->header_stamp = stuff.second;
        }    
};


// Replaces tf::Transform and tf::StampedTransform
struct Transform
{
public:
        Transform()
        {

        }
        Transform(const Transform& other)
        {
                this->rot = other.rot;
                this->trans = other.trans;
        }
        Transform(double qx, double qy, double qz, double qw, double x, double y, double z)
        {
                trans = glm::vec3(x,y,z);
                rot = glm::dquat(qw, qx, qy, qz);
        }
        // https://docs.ros.org/en/jade/api/tf/html/c++/Transform_8h_source.html
        // quat as a 3x3 matrix
        // translation as a vec3
        glm::dquat rot; // m_basis
        glm::vec3 trans; // m_origin
};

struct StampedTransform : Transform
{
public:
        StampedTransform() {}
        StampedTransform(timePoint timestamp_)
        : timestamp(timestamp_)
        {}
        StampedTransform(Transform tran, timePoint timestamp_, std::string child_frame_id_, std::string frame_id_)
        : Transform(tran), timestamp(timestamp_), child_frame_id(child_frame_id_), frame_id(frame_id_)
        {  
        }

        // https://docs.ros.org/en/jade/api/tf/html/c++/classtf_1_1StampedTransform.html
        // https://docs.ros.org/en/jade/api/tf/html/c++/transform__datatypes_8h_source.html
        // child frame, is name of this transform's frame
        std::string child_frame_id; 
        // parent frame, is name of frame this frame's transforms are with respect to
        std::string frame_id;
        // timestamp
        timePoint timestamp;

        bool operator<(const StampedTransform& rhs) const
        {
                return this->timestamp < rhs.timestamp;
        }

        StampedTransform& operator=(const StampedTransform& rhs)
        {
                this->rot = rhs.rot;
                this->trans = rhs.trans;
                this->child_frame_id = rhs.child_frame_id;
                this->frame_id = rhs.frame_id;
                this->timestamp = rhs.timestamp;
                return *this;
        }

        // Turns this Stamped Transform into a Transformation = kindr::minimal::QuatTransformation; 
        void toKindrTransformation(kindr::minimal::QuatTransformation& kindr_tf)
        {
                Eigen::Matrix<double, 3, 1> position;
                Eigen::Quaternion<double> rotation;
                Eigen::Quaterniond kindr_double(rot.w, rot.x, rot.y, rot.z);
                rotation = kindr_double.cast<double>();

                Eigen::Matrix<double, 3, 1> kindr_double_vec;
                kindr_double_vec(0) = trans.x;
                kindr_double_vec(1) = trans.y;
                kindr_double_vec(2) = trans.z;
                position = kindr_double_vec.cast<double>();

                if (rotation.w() < 0) {
                        rotation.coeffs() = -rotation.coeffs();
                }

                kindr_tf = kindr::minimal::QuatTransformationTemplate<double>(rotation, position);
        }
};

// Replaces tf::Transformer
// https://wiki.ros.org/tf
class Transformer
{
public:
        Transformer(long long max_time_seconds) 
        : max_duration_seconds(std::chrono::seconds(max_time_seconds))
        {}

        void clear()
        {
                transform_buffer.clear();
        }

        // Adds a transform to the set
        void setTransform(StampedTransform tran)
        {
                // Adding
                transform_buffer.insert(tran);

                // Updating min/max time
                if(transform_buffer.size() == 1)
                {
                        min_time = tran.timestamp;
                        max_time = tran.timestamp;
                }

                if(min_time > tran.timestamp)
                        min_time = tran.timestamp;
                if(max_time < tran.timestamp)
                        max_time = tran.timestamp;


                // Erasing if max - min exceeds the maximum allowed duration
                if(max_time - min_time > max_duration_seconds)
                {
                        std::cout<<"Are erasing in types.h/Transformer! This should only happen if have been running SLAM for >100 seconds!"<<std::endl;
                        StampedTransform min_st(min_time);
                        auto right = transform_buffer.upper_bound(min_st);
                        transform_buffer.erase(transform_buffer.begin(), right);
                        min_time = right->timestamp;
                }
        }

        // Says if lookupTransform will be valid if called with timePoint t
        bool canTransform(std::string from, std::string to, timePoint t, std::string* err)
        {
                StampedTransform test(t);
                auto right = transform_buffer.lower_bound(test);
                // Nothing is greater than t, so cannot lerp
                if(right == transform_buffer.end())
                {
                        *err = "nothing greater than timestamp";
                        return false;
                }
                // Right exactly equals t, so can just return right
                if(right->timestamp == t)
                        return true;
                // There exists another timestamp before t, so can lerp because have a left and right
                if(right!=transform_buffer.begin())
                        return true;
                // Nothing is less than t, so cannot lerp
                *err = "Nothing less than timestamp";
                return false;
        }

        // Lerps betwen poses in the set to estimate the pose at t. Guaranteed to have canTransform called beforehand.
        void lookupTransform(std::string from, std::string to, timePoint t, StampedTransform& st)
        {

                StampedTransform timestamp_want(t);
                auto right = transform_buffer.lower_bound(timestamp_want);
                // Right exactly equals t, so can just return right
                if(esvo2_core::timePointToSec(right->timestamp) == esvo2_core::timePointToSec(t))
                {
                        st = *right;
                        return;
                }
                // There exists another timestamp before t, so can lerp because have a left and right
                auto left = right;
                --left;

                double alpha = (timePointToSec(t) - timePointToSec(left->timestamp)) / (timePointToSec(right->timestamp) - timePointToSec(left->timestamp));

                Transform tranf;
                tranf.rot =  glm::slerp(left->rot, right->rot, alpha);
                tranf.trans = glm::mix(left->trans, right->trans, alpha);
                st = StampedTransform(tranf, t, left->child_frame_id, left->frame_id);
        }
        
        // Buffer
        // https://docs.ros.org/en/jade/api/tf2_ros/html/c++/buffer_8cpp_source.html 
        // sorted vector/sorted map of stamped_transform
        std::set<StampedTransform> transform_buffer;
        std::chrono::seconds max_duration_seconds;
        timePoint min_time;
        timePoint max_time;

        // Transformer
        // https://docs.ros.org/en/jade/api/tf/html/c++/classtf_1_1Transformer.html
        // https://docs.ros.org/en/jade/api/tf/html/c++/tf_8h_source.html
        // https://docs.ros.org/en/jade/api/tf/html/c++/tf_8cpp_source.html

        // ESVO2 uses only two coordinate frames, with frame_id always being the world coords and 
        // child_frame_id being some set string, so don't have to do any special tree traversal or whatever the links above
        // talk about. Can just use a std::set to store based on timestamp, then query from that.
};

} // namespace esvo2_core

#endif // ESVO2_CORE_types_H
