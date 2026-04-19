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

#include "src/util/pch.hh"
#include "src/data/EventData.hh"
#include "src/ui/Scrubber.hh"

#include <pcl/point_types.h>
// #include <pcl_ros/point_cloud.h>
// #include <pcl/filters/voxel_grid.h>

class SlamManager {
public:
    using timePoint = std::chrono::time_point<std::chrono::steady_clock>;

    SlamManager() 
    {

    }

    // Spawns threads for all the modules
    void startSlam(Scrubber* left_scrubber, Scrubber* right_scrubber, EventData* left_eventdata, EventData* right_eventdata)
    {
        // Setting the scrubbers
        this->left_scrubber = left_scrubber;
        this->right_scrubber = right_scrubber;
        this->left_eventdata = left_eventdata;
        this->right_eventdata = right_eventdata;

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
        image_representation_right_running = true;
        mapping_running = true;
        tracking_running = true;

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
        mapping.reset();
        tracking.reset();

        firstEventBatch = true;
        this->left_scrubber = nullptr;
        this->right_scrubber = nullptr;
        this->left_eventdata = nullptr;
        this->right_eventdata = nullptr;
    }

    void sendEvents()
    {
        // If SLAM is not running, just return
        if(!image_representation_left_running)
            return;
        sendEventsPerScrubber(*left_eventdata, *left_scrubber, true);
        sendEventsPerScrubber(*right_eventdata, *right_scrubber, false);
    }

private:

    void sendEventsPerScrubber(EventData &event_data, Scrubber &scrubber, bool is_left)
    {
        // If SLAM is not running, just return
        if(!image_representation_left_running)
            return;

        const std::array<std::size_t, 2> frame_dims = scrubber.get_frame_dimensions();
        const std::size_t width = frame_dims[0];
        const std::size_t height = frame_dims[1];
        const std::size_t points_buffer_size = scrubber.get_points_buffer_size();
        const std::size_t current_lower_index = scrubber.get_current_lower_index();
        const auto &event_vector = event_data.get_evt_vector_ref();

        if(!event_vector.empty() && points_buffer_size > 0)
        {
            const glm::vec4* data_ptr = event_data.get_evt_vector_ref().data() + current_lower_index;
            event_data.lock_data_vectors();

            if(firstEventBatch)
            {
                /* 
                ESVO2 requires processing timestamps based on the current absolute clock time, 
                i.e. std::chrono::time_point<std::chrono::steady_clock>::now().
                Every 10ms, imageRepresentation looks at all events less than ::now() and processes those events.
                The timestamps in the event vector are in relative time, not absolute time. In order to turn them into
                absolute time, have to do some calcuations. Idea with below calculation is to come up with some absolute "time 0"
                that can add to all the events's timestamps. It is based on the following rules:
                    - The timepoint of the last event must be <= ::now() so that all events in the batch will be processed correctly in imageRepresentation
                    - The timepoints of the events in the batch should stay in increasing order
                To do this, we just say the last event happened at ::now(). So then "time 0" would be (now - endTime).
                We only do this for the first event batch because other event batches can just use this already calculating starting time. 
                This is because future events will increase in time at the same rate that real time is passing, 
                so their time + "time 0" will never be greater than ::now(). 
                */
                timePoint now = std::chrono::steady_clock::now();
                // Casting end timestamp as a duration
                // https://docs.inivation.com/software/introduction.html: "timestamp represents the time of the start of exposure of the 
                // frame. It is represented as a Unix Timestamp in **microseconds**. Type: int64"
                std::chrono::microseconds end_duration((data_ptr + (points_buffer_size - 1))->z);
                zero_absolute_timestamp = now - end_duration;
                firstEventBatch = false;
            }

            const double duration_threshold = 1.0/1000.0;
            double curr_lower_bound_timestamp = data_ptr->z;
            double upper_bound_timestamp = curr_lower_bound_timestamp + duration_threshold;

            esvo2_core::EventArray evtArray;
            evtArray.width = width;
            evtArray.height = height;


            for(std::size_t index = 0; index < points_buffer_size; index++)
            {
                // If no longer running, just return
                if(!image_representation_left_running)
                {
                    event_data.unlock_data_vectors();
                    return;
                }

                // If reached the upper bound timestamp, can send the event array to the queues
                while(data_ptr[index].z >= upper_bound_timestamp)
                {
                    // Sending the Event Array to the queues
                    if(evtArray.events.size()!=0)
                    {
                       sendEventsToQueues(evtArray, is_left);
                       evtArray.events.clear();
                    }
                    // Updating the lower/upper bounds
                    curr_lower_bound_timestamp = upper_bound_timestamp;
                    upper_bound_timestamp = curr_lower_bound_timestamp + duration_threshold;
                }

                // Creating the event. Format is x->x, y->y, z->timestamp_relative, w->polarity
                esvo2_core::Event evt;
                evt.x = data_ptr[index].x;
                evt.y = data_ptr[index].y;
                evt.timestamp = std::chrono::microseconds(data_ptr[index].z) + zero_absolute_timestamp;
                evt.polarity = data_ptr[index].w;
                // Adding the event to the array
                evtArray.events.push_back(evt);
            }

            // If evtArray is nonempty once finish for loop, send the events to the queues
            if(evtArray.events.size()!=0)
            {
                sendEventsToQueues(evtArray, is_left);
            }
            event_data.unlock_data_vectors();
        }
    }

    void sendEventsToQueues(esvo2_core::EventArray& evtArray, bool is_left)
    {
        std::shared_ptr<esvo2_core::EventArray> final_evt_array = make_shared<esvo2_core::EventArray>();
        final_evt_array->width = evtArray.width;
        final_evt_array->height = evtArray.height;
        final_evt_array->events = std::move(evtArray.events);
        timePoint finalEventTimestamp = final_evt_array->events.at(final_evt_array->events.size()-1).timestamp;
        if(is_left)
        {
            event_left_To_IR.add(final_evt_array, finalEventTimestamp);
            image_representation_left_cv.notify_one();
            event_left_To_Map.add(final_evt_array, finalEventTimestamp);
            mapping_cv.notify_one();   
        }
        else
        {
            event_right_To_IR.add(final_evt_array, finalEventTimestamp);
            image_representation_right_cv.notify_one();
        }
    }

    void process_image_representation_left_thread(std::atomic<bool> &running, std::condition_variable& cv)
    {
        std::mutex mtx;
        unique_lock<mutex> lock(mtx);
        while(running)
        {
            cv.wait(lock, std::chrono::seconds(1));
            bool gotOne = true;
            while(gotOne)
            {
                if(!running)
                    break;

                gotOne=false;
                // checking the event queue
                event_left_To_IR.lock();
                if(event_left_To_IR.queueEmpty())
                {
                    event_left_To_IR.unlock();
                }
                else
                {
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::EventArray>, timePoint> result = event_left_To_IR.getValue();
                    event_left_To_IR.unlock();
                    std::shared_ptr<esvo2_core::EventArray> evtArray = result.first;
                    image_representation_left->eventsCallback(evtArray);
                }
            }
        }
    }

    void process_image_representation_right_thread(std::atomic<bool> &running, std::condition_variable& cv)
    {
        std::mutex mtx;
        unique_lock<mutex> lock(mtx);
        while(running)
        {
            cv.wait(lock, std::chrono::seconds(1));
            bool gotOne = true;
            while(gotOne)
            {
                if(!running)
                    break;

                gotOne=false;
                // checking the event queue
                event_right_To_IR.lock();
                if(event_right_To_IR.queueEmpty())
                {
                    event_right_To_IR.unlock();
                }
                else
                {
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::EventArray>, timePoint> result = event_right_To_IR.getValue();
                    event_right_To_IR.unlock();
                    std::shared_ptr<esvo2_core::EventArray> evtArray = result.first;
                    image_representation_right->eventsCallback(evtArray);
                }
            }
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

                // Only do so if bpoints_from_AA is false
                if(!mapping->bpoints_from_AA_)
                {
                    // checking the event queue
                    event_left_To_Map.lock();
                    if(event_left_To_Map.queueEmpty())
                    {
                        event_left_To_Map.unlock();
                    }
                    else
                    {
                        gotOne=true;
                        std::pair<std::shared_ptr<esvo2_core::EventArray>, timePoint> result = event_left_To_Map.getValue();
                        event_left_To_Map.unlock();
                        std::shared_ptr<esvo2_core::EventArray> evtArray = result.first;
                        mapping->eventsCallback(evtArray);
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

    - delete imu_factor.h b/c gnu?
    - ceres?


    - get rid of tf:: stuff
        - types.h toKindrTransformation is very probably wrong. Hard to check though without compiling
        - minkindr can be found at: https://github.com/ethz-asl/minkindr
        - Expected conversion function at: https://github.com/ethz-asl/minkindr_ros/blob/master/minkindr_conversions/include/minkindr_conversions/kindr_tf.h

    - Comment out all IMU stuff
        - comment out all the IMU code that we aren't using that isn't already disabled by the flag not being set
        - shouldn't be too bad

    - Update ros time stuff!
       - The main mapping/tracking thread functions use some ROS time stuff. Should update to use std::chrono stuff instead

    - Get rid of all include errors
        - check dockerfile for additional depenedencies if needed
            - add those dependencies
        - delete any ROS/unused imports

    - Visualization stuff
       - Figure out how to visualize PCL (point cloud library)
           - Set up a class for SLAM in the visualizer
           - Send data to that class via a reference to pc_global_ in esvo2_Mapping.cpp
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
    - Because of relative to absolute time conversion, must run SLAM on time scrub mode and in real time (no speeding up/slowing down probably)
      Can still play from file, but must play file in real time.
    - types.h toKindrTransformation is very probably wrong
    -#include <esvo2_core/DVS_MappingStereoConfig.h> in Mapping.h may be necessary
    - ref_.vPointXYZPtr_.push_back(&(*PointXYZ_begin_it)); // Copy the pointer of the pointXYZ in Tracking.cpp is very sus
      but I think it is correct
*/