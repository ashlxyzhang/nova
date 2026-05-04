#pragma once

// FROM STD library
#include <atomic>
#include <condition_variable>
#include <thread>

// From NOVA
#include "data/EventData.hh"
#include "data/Scrubber.hh"
#include "util/pch.hh"

// FROM SLAM
#include "data_passing.hh"
#include "multi_data_passing.hh"
#include "esvo2_core/esvo2_Mapping.h"
#include "esvo2_core/esvo2_Tracking.h"
#include "esvo2_core/tools/types.h"
#include "image_representation/ImageRepresentation.h"

// From Dependencies
#include <opencv2/core/mat.hpp>
#include <pcl/point_types.h>

namespace nova {

class SlamManager
{
    public:
        using timePoint = std::chrono::time_point<std::chrono::steady_clock>;

        enum class SlamConfigFiles
        {
            IR_Left=0, 
            IR_Right, 
            Tracking, 
            Mapping, 
            Camera_Left, 
            Camera_Right
        };

        struct StartSlamParameters
        {
            Scrubber *left_scrubber;
            Scrubber *right_scrubber;
            EventData *left_eventdata;
            EventData *right_eventdata;
        };

        SlamManager();

        // Starts slam
        void startSlam(StartSlamParameters params);

        // Stops slam
        void stopSlam();

        bool isRunning() const { return image_representation_left_running; }

        // Sends left/right events from scrubbers to the SLAM threads
        void send_events();

        void set_config_file(std::string file_path);
        std::string get_config_file_path(SlamConfigFiles type);

        void set_curr_file_type(SlamConfigFiles curr_type)
        {
            current_config_file_type = curr_type;
        }

        // Get the PCL point cloud for NOVA Visualizer
        // points to pc_filtered_ (the filtered point cloud for one frame)
        std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBL>> get_viz_pointcloud()
        {
            if (!mapping_running || !mapping)
                return nullptr;
            return mapping->get_viz_pointcloud();
        }

        // Get the PCL global point cloud for NOVA Visualizer
        // points to pc_filtered_global_ (the global point cloud)
        std::shared_ptr<pcl::PointCloud<pcl::PointXYZ>> get_viz_global_pointcloud()
        {
            if (!mapping_running || !mapping)
                return nullptr;
            return mapping->get_viz_global_pointcloud();
        }

        bool were_pointclouds_updated()
        {
            if (!mapping_running || !mapping)
                return false;
            return mapping->were_pointclouds_updated();
        }

        // Get path info for the NOVA visualizer
        std::shared_ptr<std::vector<esvo2_core::PoseStamped>> get_viz_path()
        {
            if (!tracking_running || !tracking)
                return nullptr;
            return tracking->get_viz_path();
        }

        bool was_path_updated()
        {
            if (!tracking_running || !tracking)
                return false;
            return tracking->was_path_updated();
        }

    private:
        // Sends events to the SLAM threads based on one scrubber
        void sendEventsPerScrubber(EventData &event_data, Scrubber &scrubber, bool is_left);

        // Helper to send an eventarray to its corresponding queues
        void sendEventsToQueues(esvo2_core::EventArray &evtArray, bool is_left);

        // Thread function to send data to image_representation for the left camera
        void process_image_representation_left_thread(std::atomic<bool> &running, std::condition_variable &cv);

        // Thread function to send data to image_representation for the left camera
        void process_image_representation_right_thread(std::atomic<bool> &running, std::condition_variable &cv);

        // Thread function to send data to mapping
        void process_mapping_thread(std::atomic<bool> &running, std::condition_variable &cv);

        // Thread function to send data to tracking
        void process_tracking_thread(std::atomic<bool> &running, std::condition_variable &cv);

        //! Modules (uses unique_ptr for RAII, prevents accidental copies and double deletes, and allows
        //! for modules to hold references to one another)
        std::unique_ptr<image_representation::ImageRepresentation> image_representation_left;
        std::unique_ptr<image_representation::ImageRepresentation> image_representation_right;
        std::unique_ptr<esvo2_core::esvo2_Mapping> mapping;
        std::unique_ptr<esvo2_core::esvo2_Tracking> tracking;

        // For calibration
        SlamConfigFiles current_config_file_type;
        std::string IR_Left_yaml_path="../src/SLAM/image_representation/cfg/image_representation_fast.yaml";
        std::string IR_Right_yaml_path="../src/SLAM/image_representation/cfg/image_representation_fast_r.yaml";
        std::string Tracking_yaml_path="../src/SLAM/esvo2_core/cfg/tracking/tracking_dsec_AA.yaml";
        std::string Mapping_yaml_path="../src/SLAM/esvo2_core/cfg/mapping/mapping_dsec_AA.yaml";
        std::string left_camera_yaml_path="../src/SLAM/esvo2_core/calib/dsec/zurich_city_04_a/left.yaml";
        std::string right_camera_yaml_path="../src/SLAM/esvo2_core/calib/dsec/zurich_city_04_a/right.yaml";
        YAML::Node yaml_IR_Left_config;
        YAML::Node yaml_IR_Right_config;
        YAML::Node yaml_Track_config;
        YAML::Node yaml_Map_config;
        YAML::Node _config;

        // For sending events
        timePoint zero_absolute_timestamp;
        // double last_processed_event_time_left = 0;
        // double last_processed_event_time_right = 0;
        std::size_t last_processed_event_idx_left = 0;
        std::size_t last_processed_event_idx_right = 0;
        bool firstEventBatch = true;
        Scrubber *left_scrubber = nullptr;
        Scrubber *right_scrubber = nullptr;
        EventData *left_eventdata = nullptr;
        EventData *right_eventdata = nullptr;

        //! Worker threads run until their respective boolean flag is set false (see SDL_Quit)
        // These threads just check if their queues got new data in them, then call the corresponding callback functions
        std::atomic<bool> image_representation_left_running = false;
        std::thread image_representation_left_thread;
        std::condition_variable image_representation_left_cv;

        std::atomic<bool> image_representation_right_running = false;
        std::thread image_representation_right_thread;
        std::condition_variable image_representation_right_cv;

        std::atomic<bool> mapping_running = false;
        std::thread mapping_thread;
        std::condition_variable mapping_cv;

        std::atomic<bool> tracking_running = false;
        std::thread tracking_thread;
        std::condition_variable tracking_cv;

        // Queues for data passing
        // ----From IR----
        // DataPassingDeque<cv::Mat> time_surface_right_IR_to_Track; //unused in original implentation
        DataPassingDeque<cv::Mat> AA_left_IR_to_Map;

        // Multi Data Passing
        // TSleft, TSnegative, dx, dy; All left!
        MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat> multi_to_Track;
        // DataPassingDeque<cv::Mat> time_surface_left_IR_to_Track; // part of multidata
        // DataPassingDeque<cv::Mat> neg_time_surface_left_IR_to_Track; // part of multidata
        // DataPassingDeque<cv::Mat> neg_time_surface_dx_left_IR_to_Track; // part of multidata
        // DataPassingDeque<cv::Mat> neg_time_surface_dy_left_IR_to_Track;// part of multidata

        // TSleft, TSright, AA MAP, TS neg, dx, dy; ALL left except for TSright!
        MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat> multi_to_Map;
        // DataPassingDeque<cv::Mat> time_surface_left_IR_to_Map; // part of multidata
        // DataPassingDeque<cv::Mat> time_surface_right_IR_to_Map; // part of multidata
        // DataPassingDeque<cv::Mat> AA_map_left_IR_to_Map; // part of multidata
        // DataPassingDeque<cv::Mat> neg_time_surface_left_IR_to_Map; // part of multidata
        // DataPassingDeque<cv::Mat> neg_time_surface_dx_left_IR_to_Map; // part of multidata
        // DataPassingDeque<cv::Mat> neg_time_surface_dy_left_IR_to_Map;// part of multidata

        // ----From Mapping----
        DataPassingDeque<esvo2_core::VBaBg> v_ba_bg_Map_to_Track;
        DataPassingDeque<pcl::PointCloud<pcl::PointXYZRGBL>> pointcloud_Map_to_Track;

        // ----From Tracking----
        DataPassingDeque<esvo2_core::PoseStamped> stamped_pose_Track_to_Map;
        DataPassingDeque<esvo2_core::PoseStamped> stamped_pose_Track_to_Track;

        // Not used by the modules, just for visualization?
        // DataPassingDeque<cv::Mat> inverse_depth_map_Mapping_to_Viz;
        // DataPassingDeque<pcl::PointCloud<pcl::PointXYZRGBL>> pointcloud_Map_to_Viz;
        // DataPassingDeque<pcl::PointCloud<pcl::PointXYZRGBL>> pointcloud_filtered_Map_to_Viz;
        // DataPassingDeque<pcl::PointCloud<pcl::PointXYZRGBL>> pointcloud_global_Map_to_Viz;
        // DataPassingDeque<nav_msgs::Path> trajectory_Track_to_Viz;
        // DataPassingDeque<cv::Mat> reproj_map_left_Track_to_Viz;

        // ---From outside (EVENTS/IMU)---
        DataPassingDeque<esvo2_core::EventArray> event_left_To_IR;
        DataPassingDeque<esvo2_core::EventArray> event_right_To_IR;
        DataPassingDeque<esvo2_core::EventArray> event_left_To_Map;
        // /imu/data -> Mapping
        // /imu/data -> Tracking
};

// -------TODO---------
/*
    - Visualization stuff
       - Probably need to add pose visualization too because can be kinda confusing on where the path is for the camera
            - just a thick line connecting the transform positions is probably fine
       - There is a bug where filtered point cloud does not spawn in correct location once swap back from global.
          The camera is like not in the correct spot for it and views it from the side instead of from the front.
       
    - Controls/GUI
        - add start slam button that shows up if have two data sources
        - figure out how user should select all the YAML files + output file for pose stuff if they want
        - add toggle to view global point cloud vs just filtered
*/

// -----------NOTES on sus things---------
/*
    - (10,10) for multi_data_passing queue sizes might be incorrect, but it should be fine
    - types.h toKindrTransformation is a bit sus
    - EventQueue was originally called EventBuffer and was a vector of Event* before started refactoring. Now it is a deque of events.
    I don't think it affects anything but it might be an issue
*/

} // namespace nova