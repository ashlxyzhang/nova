#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "src/SLAM/image_representation/include/image_representation/ImageRepresentation.h"
#include "src/SLAM/esvo2_core/include/esvo2_core/esvo2_Tracking.h"
#include "src/SLAM/esvo2_core/include/esvo2_core/esvo2_Mapping.h"
#include "data_passing.h"
#include "multi_data_passing.h"
#include <opencv2/core/mat.hpp>
#include "types.h"

#include <pcl/point_types.h>
// #include <pcl_ros/point_cloud.h>
// #include <pcl/filters/voxel_grid.h>

class SlamManager {
public:
    using timePoint = std::chrono::time_point<std::chrono::steady_clock>;

    SlamManager() {}

    // Spawns threads for all the modules
    void startSlam()
    {
        // Setting up the queues
        AA_left_IR_to_Map = DataPassingDeque<cv::Mat>(1000, &mapping_cv);
        v_ba_bg_Map_to_Track = DataPassingDeque<esvo2_core::VBaBg>(1000, &tracking_cv);
        pointcloud_Map_to_Track = DataPassingDeque<pcl::PointCloud<pcl::PointXYZRGBL>>(1000, &tracking_cv);
        stamped_pose_Track_to_Map = DataPassingDeque<esvo2_core::PoseStamped>(1000, &mapping_cv);
        stamped_pose_Track_to_Track = DataPassingDeque<esvo2_core::PoseStamped>(1000, &tracking_cv);

        multi_to_Track = MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat>(10, 10, 4, &tracking_cv);
        multi_to_Map = MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat>(10, 10, 6, &mapping_cv); 
        
        // Updating bools because we are currently running
        image_representation_left_running = true;
        image_representation_right_thread = true;
        mapping_thread = true;
        tracking_thread = true;

        // The module's constructors create and detach a new thread that manages their respective processes
        image_representation_left = std::make_unique<ImageRepresentation>();
        image_representation_right = std::make_unique<ImageRepresentation>();
        mapping = std::make_unique<esvo2_Mapping>();
        tracking = std::make_unique<esvo2_Tracking>();

        // Launching the threads to handle the queues/passing of data
        image_representation_left_thread = std::thread(process_image_representation_left_thread, std::ref(image_representation_left_running), std::ref(image_representation_left_cv));
        image_representation_left_thread = std::thread(process_image_representation_right_thread, std::ref(image_representation_right_running), std::ref(image_representation_right_cv));
        mapping_thread = std::thread(process_mapping_thread, std::ref(mapping_running), std::ref(mapping_cv));
        tracking_thread = std::thread(process_tracking_thread, std::ref(tracking_running), std::ref(tracking_cv));
    }

    void stopSlam()
    {
        // joining all the threads
        image_representation_left_running = false;
        image_representation_left_thread.join();

        image_representation_right_running = false;
        image_representation_right_thread.join();

        mapping_running = false;
        mapping_thread.join();

        tracking_running = false;
        tracking_thread.join();

        // Reset (deleting) all of the modules
        image_representation_left.reset();
        image_representation_right.reset();
        mapping_thread.reset();
        tracking_thread.reset();
    }

private:

    void process_image_representation_left_thread(std::atomic<bool> &running, std::condition_variable& cv)
    {
        std::mutex mtx;
        unique_lock<mutex> lock(mtx);
        while(running)
        {
            // TODO check for events here then call ImageRepresentation::eventsCallback
            // Don't lock for each event!
        }
    }

    void process_image_representation_right_thread(std::atomic<bool> &running, std::condition_variable& cv)
    {
        std::mutex mtx;
        unique_lock<mutex> lock(mtx);
        while(running)
        {
            // TODO check for events here then call ImageRepresentation::eventsCallback
            // Don't lock for each event!
        }
    }

    void process_mapping_thread(std::atomic<bool> &running, std::condition_variable& cv)
    {
        std::mutex mtx;
        unique_lock<mutex> lock(mtx);
        while(running)
        {
            cv.wait(lock, std::chrono::seconds(1));
            
            // Get data until all queues are empty
            bool gotOne = true;
            while(gotOne)
            {
                if(!running)
                    break;

                gotOne=false;

                // checking the multidata queue
                multi_to_Map.lock();
                if(multi_to_Map.queueEmpty())
                { 
                    multi_to_Map.unlock();
                }
                else
                {
                    gotOne=true;
                    auto result = multi_to_Map.getValues();
                    multi_to_Map.unlock();
                    // TS left, TS right, AA MAP, TS neg, dx, dy
                    std::pair<std::shared_ptr<cv::Mat>, timePoint> ts_left = std::get<0>(result);
                    std::pair<std::shared_ptr<cv::Mat>, timePoint> ts_right = std::get<1>(result);
                    std::pair<std::shared_ptr<cv::Mat>, timePoint> aa_map = std::get<2>(result);
                    std::pair<std::shared_ptr<cv::Mat>, timePoint> ts_neg = std::get<3>(result);
                    std::pair<std::shared_ptr<cv::Mat>, timePoint> dx = std::get<4>(result);
                    std::pair<std::shared_ptr<cv::Mat>, timePoint> dy = std::get<5>(result);

                    esvo2_core::ImagePtr time_surface_left(ts_left);
                    esvo2_core::ImagePtr time_surface_right(ts_right);
                    esvo2_core::ImagePtr AA_map(aa_map);
                    esvo2_core::ImagePtr time_surface_negative(ts_neg);
                    esvo2_core::ImagePtr time_surface_negative_dx(dx);
                    esvo2_core::ImagePtr time_surface_negative_dy(dy);

                    mapping->timeSurfaceCallback(time_surface_left, time_surface_right, AA_map, time_surface_negative, time_surface_negative_dx, time_surface_negative_dy);
                }


                // checking the stamped_pose_Track_to_Map
                stamped_pose_Track_to_Map.lock();
                if(stamped_pose_Track_to_Map.queueEmpty())
                {
                    stamped_pose_Track_to_Map.unlock();
                }
                else
                {
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::PoseStamped>, timePoint> result = stamped_pose_Track_to_Map.getValue();
                    stamped_pose_Track_to_Map.unlock();

                    std::shared_ptr<esvo2_core::PoseStamped> stamped_pose = result.first;
                    mapping->stampedPoseCallback(stamped_pose);
                }

                // checking the AA_left_IR_to_Map. Only do so if bpoints_from_AA is true
                if(mapping->bpoints_from_AA_)
                {
                    AA_left_IR_to_Map.lock();
                    if(AA_left_IR_to_Map.queueEmpty())
                    {
                        AA_left_IR_to_Map.unlock();
                    }
                    else
                    {
                        gotOne=true;
                        std::pair<std::shared_ptr<cv::Mat>, timePoint> result = AA_left_IR_to_Map.getValue();
                        AA_left_IR_to_Map.unlock();

                        esvo2_core::ImagePtr AA_left(result);
                        mapping->AACallback(AA_left);
                    }
                }
            }
        }
    }

    void process_tracking_thread(std::atomic<bool> &running, std::condition_variable& cv)
    {
        std::mutex mtx;
        unique_lock<mutex> lock(mtx);
        while(running)
        {
            cv.wait(lock, std::chrono::seconds(1));
            
            // Get data until all queues are empty
            bool gotOne = true;
            while(gotOne)
            {
                if(!running)
                    break;

                gotOne=false;

                // checking the multidata queue
                multi_to_Track.lock();
                if(multi_to_Track.queueEmpty())
                { 
                    multi_to_Track.unlock();
                }
                else
                {
                    gotOne=true;
                    auto result = multi_to_Track.getValues();
                    multi_to_Track.unlock();
                    // TSleft, TSnegative, dx, dy;
                    std::pair<std::shared_ptr<cv::Mat>, timePoint> ts_left = std::get<0>(result);
                    std::pair<std::shared_ptr<cv::Mat>, timePoint> ts_neg = std::get<1>(result);
                    std::pair<std::shared_ptr<cv::Mat>, timePoint> dx = std::get<2>(result);
                    std::pair<std::shared_ptr<cv::Mat>, timePoint> dy = std::get<3>(result);

                    esvo2_core::ImagePtr time_surface_left(ts_left);
                    esvo2_core::ImagePtr time_surface_negative(ts_neg);
                    esvo2_core::ImagePtr time_surface_negative_dx(dx);
                    esvo2_core::ImagePtr time_surface_negative_dy(dy);

                    tracking->timeSurface_NegaTS_Callback(time_surface_left, time_surface_negative, time_surface_negative_dx, time_surface_negative_dy);
                }


                // checking the v_ba_bg_Map_to_Track
                v_ba_bg_Map_to_Track.lock();
                if(v_ba_bg_Map_to_Track.queueEmpty())
                {
                    v_ba_bg_Map_to_Track.unlock();
                }
                else
                {
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::VBaBg>, timePoint> result = v_ba_bg_Map_to_Track.getValue();
                    v_ba_bg_Map_to_Track.unlock();

                    std::shared_ptr<esvo2_core::VBaBg> msg = result.first;
                    tracking->VBaBgCallback(msg);
                }

                // checking the pointcloud_Map_to_Track
                pointcloud_Map_to_Track.lock();
                if(pointcloud_Map_to_Track.queueEmpty())
                {
                    pointcloud_Map_to_Track.unlock();
                }
                else
                {
                    gotOne=true;
                    std::pair<std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBL>>, timePoint> result = pointcloud_Map_to_Track.getValue();
                    pointcloud_Map_to_Track.unlock();

                    tracking->refMapCallback(result);
                }

                //stamped_pose_Track_to_Track
                stamped_pose_Track_to_Track.lock();
                if(stamped_pose_Track_to_Track.queueEmpty())
                {
                    stamped_pose_Track_to_Track.unlock();
                }
                else
                {
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::PoseStamped>, timePoint> result = stamped_pose_Track_to_Track.getValue();
                    stamped_pose_Track_to_Track.unlock();

                    std::shared_ptr<esvo2_core::PoseStamped> stamped_pose = result.first;
                    tracking->stampedPoseCallback(stamped_pose);
                }
            }
        }
    }
    
    //! Modules (uses unique_ptr for RAII, prevents accidental copies and double deletes, and allows
    //! for modules to hold references to one another)
    std::unique_ptr<ImageRepresentation> image_representation_left;
    std::unique_ptr<ImageRepresentation> image_representation_right;
    std::unique_ptr<esvo2_Mapping> mapping;
    std::unique_ptr<esvo2_Tracking> tracking;

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
    //    events_left -> Mapping
    //    /imu/data -> Mapping
    // /imu/data -> Tracking
    // events/left and events/right -> image representation
    
};

// -------TODO---------
/* 
    - Do event stuff!
        - Need to check with Ryan to make sure scrubbers are ready.
        - Also need to check with Ashley to make sure I am understanding this right
        - maybe just make another thread that reads from the left and right camera scrubbers, assembles events into an event array 
          of size according to what the google doc says (all events in a 0.001 second interval), then sends them to 3 queues
        - update process_image_representation_left_thread and process_image_representation_right_thread to process the left/right queues
        - update process_mapping_thread() to process the left_map queue
        - do it after data acquasition update

    - Comment out all IMU stuff
        - comment out all the IMU code that we aren't using that isn't already disabled by the flag not being set
        - shouldn't be too bad

    - Update ros time stuff!
       - The main mapping/tracking thread functions use some ROS time stuff. Should update to use std::chrono stuff instead

    - Visualization stuff
       - Figure out how to visualize PCL (point cloud library)
           - Set up a class for SLAM in the visualizer
           - Send data to that class from the Mapping pointcloud_global2 publisher??
       - If have time, can add ways to visualize the other stuff 
         - All visualization queues are commented out above, so can readd them if want to 
         - visualize the other types of point clouds 
         - visualize the pose/trajectory info 
         - visualize the various cv::Mat images produced (like time surface, aa, inverse depth, etc.)
        
    - Try to compile
        - Update cmake lists
        - Fix all compile errors once get them
        - Hope it runs properly
        - Fix bugs when it doesn't run properly
           - Might be worth to visualize the cv::Mat inverse depth if need to debug something
*/

// -----------NOTES on sus things---------
/*
    - (10,10) for multi_data_passing queue sizes might be incorrect, but it should be fine
    - I am pretty sure stuff in the subscribe callback functions treat variables as a const, 
        so I have been treating it as okay to send the same shared ptr to multiple queues
*/