#include <thread>
#include <atomic>
#include <condition_variable>

#include "util/pch.hh"
#include "data/EventData.hh"
#include "ui/Scrubber.hh"

#include "image_representation/ImageRepresentation.h"
#include "esvo2_core/esvo2_Tracking.h"
#include "esvo2_core/esvo2_Mapping.h"
#include "esvo2_core/tools/types.h"
#include "data_passing.hh"
#include "multi_data_passing.hh"

#include <opencv2/core/mat.hpp>
#include <pcl/point_types.h>

class SlamManager {
public:
    using timePoint = std::chrono::time_point<std::chrono::steady_clock>;

    SlamManager() ;

    // Starts slam
    void startSlam(Scrubber* left_scrubber, Scrubber* right_scrubber, EventData* left_eventdata, EventData* right_eventdata);

    // Stops slam
    void stopSlam();

    // Sends left/right events from scrubbers to the SLAM threads
    void send_events();

private:

    // Sends events to the SLAM threads based on one scrubber
    void sendEventsPerScrubber(EventData &event_data, Scrubber &scrubber, bool is_left);

    // Helper to send an eventarray to its corresponding queues
    void sendEventsToQueues(esvo2_core::EventArray& evtArray, bool is_left);

    // Thread function to send data to image_representation for the left camera
    void process_image_representation_left_thread(std::atomic<bool> &running, std::condition_variable& cv);

    // Thread function to send data to image_representation for the left camera
    void process_image_representation_right_thread(std::atomic<bool> &running, std::condition_variable& cv);

    // Thread function to send data to mapping
    void process_mapping_thread(std::atomic<bool> &running, std::condition_variable& cv);

    // Thread function to send data to tracking
    void process_tracking_thread(std::atomic<bool> &running, std::condition_variable& cv);
    
    //! Modules (uses unique_ptr for RAII, prevents accidental copies and double deletes, and allows
    //! for modules to hold references to one another)
    std::unique_ptr<image_representation::ImageRepresentation> image_representation_left;
    std::unique_ptr<image_representation::ImageRepresentation> image_representation_right;
    std::unique_ptr<esvo2_core::esvo2_Mapping> mapping;
    std::unique_ptr<esvo2_core::esvo2_Tracking> tracking;

    // For sending events
    timePoint zero_absolute_timestamp;
    bool firstEventBatch = true;
    Scrubber* left_scrubber = nullptr;
    Scrubber* right_scrubber = nullptr;
    EventData* left_eventdata = nullptr;
    EventData* right_eventdata = nullptr;

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
        - Expected conversion function at: https://github.com/ethz-asl/minkindr_ros/blob/master/minkindr_conversions/include/minkindr_conversions/kindr_tf.h

     - UPdate slam_manager.cpp to have correct parameters to constructors of the classes under the comment:
            // The module's constructors create and detach a new thread that manages their respective processes

     - Try to compile
        - Update cmake lists X
        - Fix all compile errors once get them (in progress)

    - Hope it runs properly
        - Fix bugs when it doesn't run properly
           - Might be worth to visualize the cv::Mat inverse depth if need to debug something

    - Datasets
        -https://dsec.ifi.uzh.ch/dsec-datasets/download/
            - Is promising, has h5 files, but are not in standard prophesee format so can't auto convert to .dat.
                Can probably edit files to get around that though. This is probably most promising solution.
        -https://daniilidis-group.github.io/mvsec/download/
            - Has h5 files, but left/right are grouped together, so would be kinda annoying to separate

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
    - Because of relative to absolute time conversion, must run SLAM on time scrub mode and in real time (no speeding up/slowing down probably)
      Can still play from file, but must play file in real time.
    - types.h toKindrTransformation is very probably wrong
    -#include <esvo2_core/DVS_MappingStereoConfig.h> in Mapping.h may be necessary
    - ref_.vPointXYZPtr_.push_back(&(*PointXYZ_begin_it)); // Copy the pointer of the pointXYZ in Tracking.cpp is very sus
      but I think it is correct
    - Mapping.cpp does vEventsPtr_left_SGM_.push_back(&(*ev_begin_it)); because vEventsPtr_left_SGM_ holds pointers. This replicates functionality 
      of ESVO2 I think but is pretty sus.
    - EventQueue was originally called EventBuffer and was a vector of Event* before started refactoring. I don't think it affects anything
      but it might be an issue 
*/