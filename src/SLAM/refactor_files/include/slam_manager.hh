#include <atomic>
#include <condition_variable>
#include <thread>

#include "data/EventData.hh"
#include "ui/Scrubber.hh"
#include "util/pch.hh"

#include "data_passing.hh"
#include "esvo2_core/esvo2_Mapping.h"
#include "esvo2_core/esvo2_Tracking.h"
#include "esvo2_core/tools/types.h"
#include "image_representation/ImageRepresentation.h"
#include "multi_data_passing.hh"

#include <opencv2/core/mat.hpp>
#include <pcl/point_types.h>

class SlamManager
{
    public:
        using timePoint = std::chrono::time_point<std::chrono::steady_clock>;

        struct StartSlamParameters
        {
                Scrubber *left_scrubber;
                Scrubber *right_scrubber;
                EventData *left_eventdata;
                EventData *right_eventdata;
                std::string IR_Left_yaml_path;
                std::string IR_Right_yaml_path;
                std::string Tracking_yaml_path;
                std::string Mapping_yaml_path;
                std::string left_camera_yaml_path;
                std::string right_camera_yaml_path;
        };

        SlamManager();

        // Starts slam
        void startSlam(StartSlamParameters params);

        // Stops slam
        void stopSlam();

        // Sends left/right events from scrubbers to the SLAM threads
        void send_events();

        // Get the PCL point cloud for NOVA Visualizer
        // points to pc_filtered_ (the filtered point cloud for one frame)
        std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBL>> get_viz_pointcloud()
        {
            if (!mapping_running || !mapping)
                return nullptr;
            return mapping->get_viz_pointcloud();
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
        YAML::Node yaml_IR_Left_config;
        YAML::Node yaml_IR_Right_config;
        YAML::Node yaml_Track_config;
        YAML::Node yaml_Map_config;
        YAML::Node _config;

        // For sending events
        timePoint zero_absolute_timestamp;
        double last_processed_event_time = 0;
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
        // DataPassingDeque<cv::Mat> time_surface_right_IR_to_Track; //unused :C
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
    - Fix types.h toKindrTransformation, which is very probably wrong. Hard to check though without compiling
        - minkindr can be found at: https://github.com/ethz-asl/minkindr
        - Expected conversion function at:
   https://github.com/ethz-asl/minkindr_ros/blob/master/minkindr_conversions/include/minkindr_conversions/kindr_tf.h

    - Finish setting up params and call start_slam in main.cc
        - Hope it runs properly
        - Fix bugs when it doesn't run properly
        - Might be worth to visualize the cv::Mat inverse depth if need to debug something

    - Visualization stuff
       - Figure out how to visualize PCL (point cloud library)
           - Set up a class for SLAM in the visualizer
           - Send data to that class via a reference to pc_global_ in esvo2_Mapping.cpp
       - If have time, can add ways to visualize the other stuff
         - All visualization queues are commented out above, so can readd them if want to
         - visualize the other types of point clouds
         - visualize the pose/trajectory info
         - visualize the various cv::Mat images produced (like time surface, aa, inverse depth, etc.)
*/

// -----------NOTES on sus things---------
/*
    - (10,10) for multi_data_passing queue sizes might be incorrect, but it should be fine
    - I am pretty sure stuff in the subscribe callback functions treat variables as a const,
        so I have been treating it as okay to send the same shared ptr to multiple queues
    - Because of relative to absolute time conversion, must run SLAM on time scrub mode and in real time (no speeding
   up/slowing down probably) Can still play from file, but must play file in real time.
    - types.h toKindrTransformation is very probably wrong
    -#include <esvo2_core/DVS_MappingStereoConfig.h> in Mapping.h may be necessary
    - ref_.vPointXYZPtr_.push_back(&(*PointXYZ_begin_it)); // Copy the pointer of the pointXYZ in Tracking.cpp is very
   sus but I think it is correct
    - Mapping.cpp does vEventsPtr_left_SGM_.push_back(&(*ev_begin_it)); because vEventsPtr_left_SGM_ holds pointers.
   This replicates functionality of ESVO2 I think but is pretty sus.
    - EventQueue was originally called EventBuffer and was a vector of Event* before started refactoring. I don't think
   it affects anything but it might be an issue
    - Dataset may have repeating events because of how query it?
*/

// ---------CONFIG NOTES----------
/*
- I don't think can do defaults? Because need the .yaml file in their file system somewhere

Image Representation
    - Does not depend on camera type. (If camera type has lots of pixels, may want the less expensive version though).
    - More expensive, better results -> image_representation_fast_40hz.yaml
    - Less expensive, worse results -> image_representation_fast.yaml
    - _r means for right camera
    - Default should be less expensive probably because NOVA as a whole is already expensive

CameraSystem
    - Depends on camera type
    - Path is passed in constructor of image representaiton
    - Stores left/right camera configurations

Mapping
    - Does not depend on camera type. (If camera type has lots of pixels, may want the less expensive version though).
    - kinda up to you idk what all the settings do
    - default should just be whichever of the given ones works best

Tracking
    - Does not depend on camera type. (If camera type has lots of pixels, may want the less expensive version though).
    - kinda up to you idk what all the settings do
    - default should just be whichever of the given ones works best
    - Need to set USE_IMU  to false!


*/