// Better type definitions are in types.h

#ifndef MSG_TYPES_H
#define MSG_TYPES_H

#include <chrono>
#include <memory>
#include <vector>
#include <string>
#include <opencv2/core/core.hpp>

namespace std_msgs
{
    // https://docs.ros.org/en/noetic/api/std_msgs/html/msg/Header.html
    struct Header
    {
        // is an ID. I don't think it is explicitly used
        unsigned int sequence;
        
        // The timestamp
        std::chrono::time_point<std::chrono::steady_clock> stamp;

        double toSeconds()
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(stamp.time_since_epoch()).count();
        }
        
        // 0: no frame
        // 1: global frame
        std::string frame_id;
    };
};

namespace geometry_msgs
{

    struct Point
    {
        double x;
        double y;
        double z;
    };

    struct Quaternion
    {
        double x;
        double y;
        double z;
        double w;
    };

    
    struct Pose
    {
        Point position;
        Quaternion orientation;
    };

    struct PoseStamped
    {
        // https://docs.ros.org/en/noetic/api/geometry_msgs/html/msg/PoseStamped.html
        // https://docs.ros.org/en/diamondback/api/geometry_msgs/html/PoseStamped_8h_source.html line 201
        std_msgs::Header header;
        Pose pose;
    };

    using PoseStampedConstPtr = std::shared_ptr<PoseStamped const>;    
};

namespace nav_msgs
{
    struct Path
    {
        std_msgs::Header header;
        std::vector<geometry_msgs::PoseStamped> poses;
    };  
};

namespace sensor_msgs
{
    struct ImageConstPtr
    {
        std_msgs::Header header;
        std::shared_ptr<cv::Mat> image;
        public:
            ImageConstPtr(std::shared_ptr<cv::Mat> image, std::chrono::time_point<std::chrono::steady_clock> time)
            {
                this->image = image;
                this->header.stamp = time;
            }
            ImageConstPtr(std::pair<std::shared_ptr<cv::Mat>, std::chrono::time_point<std::chrono::steady_clock>> stuff)
            {
                this->image = stuff.first;
                this->header.stamp = stuff.second;
            }
            
    };
};

#endif