#include "slam_manager.hh"

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
#include "esvo2_core/tools/SystemStatus.h"

#include <opencv2/core/mat.hpp>
#include <pcl/point_types.h>


    SlamManager::SlamManager() {}

    // Spawns threads for all the modules
    void SlamManager::startSlam(StartSlamParameters params)
    {
        std::cout<<"Before YAML"<<std::endl;
        // Loading YAML config files
        yaml_IR_Left_config = YAML::LoadFile(params.IR_Left_yaml_path);
        std::cout<<"yaml 1 done"<<std::endl;
        yaml_IR_Right_config = YAML::LoadFile(params.IR_Right_yaml_path);
        std::cout<<"yaml2 done"<<std::endl;
        yaml_Track_config = YAML::LoadFile(params.Tracking_yaml_path);
         std::cout<<"yaml3 done"<<std::endl;
        yaml_Map_config = YAML::LoadFile(params.Mapping_yaml_path);
         std::cout<<"yaml4 done"<<std::endl;

        // Setting the scrubbers
        this->left_scrubber = params.left_scrubber;
        this->right_scrubber = params.right_scrubber;
        this->left_eventdata = params.left_eventdata;
        this->right_eventdata = params.right_eventdata;

        std::cout<<"about to start the queues"<<std::endl;
        // Setting up the queues
        AA_left_IR_to_Map = DataPassingDeque<cv::Mat>(1000, &mapping_cv);
        v_ba_bg_Map_to_Track = DataPassingDeque<esvo2_core::VBaBg>(1000, &tracking_cv);
        pointcloud_Map_to_Track = DataPassingDeque<pcl::PointCloud<pcl::PointXYZRGBL>>(1000, &tracking_cv);
        stamped_pose_Track_to_Map = DataPassingDeque<esvo2_core::PoseStamped>(1000, &mapping_cv);
        stamped_pose_Track_to_Track = DataPassingDeque<esvo2_core::PoseStamped>(1000, &tracking_cv);
        event_left_To_IR = DataPassingDeque<esvo2_core::EventArray>(1000, &image_representation_left_cv);
        event_right_To_IR = DataPassingDeque<esvo2_core::EventArray>(1000, &image_representation_right_cv);
        event_left_To_Map = DataPassingDeque<esvo2_core::EventArray>(1000, &mapping_cv);

        multi_to_Track = MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat>(10, 10, 4, &tracking_cv);
        multi_to_Map = MultiDataPassing<cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat, cv::Mat>(10, 10, 6, &mapping_cv); 
        
        // Updating bools because we are currently running
        image_representation_left_running = true;
        image_representation_right_running = true;
        mapping_running = true;
        tracking_running = true;

        std::cout<<"About to do constructors"<<std::endl;
        // The module's constructors create and detach a new thread that manages their respective processes
        image_representation_left = std::make_unique<image_representation::ImageRepresentation>(
                image_representation_left_running, yaml_IR_Left_config, params.left_camera_yaml_path, 
                multi_to_Track, multi_to_Map, AA_left_IR_to_Map);
        std::cout<<"IR right "<<std::endl;
        image_representation_right = std::make_unique<image_representation::ImageRepresentation>(
                image_representation_right_running, yaml_IR_Right_config, params.right_camera_yaml_path, 
                multi_to_Track, multi_to_Map, AA_left_IR_to_Map);
            std::cout<<"Mapping"<<std::endl;
        mapping = std::make_unique<esvo2_core::esvo2_Mapping>(
                mapping_running, yaml_Map_config, params.left_camera_yaml_path, params.right_camera_yaml_path,
                v_ba_bg_Map_to_Track, pointcloud_Map_to_Track);
            std::cout<<"Tracking"<<std::endl;
        tracking = std::make_unique<esvo2_core::esvo2_Tracking>(
                tracking_running, yaml_Track_config, params.left_camera_yaml_path, params.right_camera_yaml_path,
                stamped_pose_Track_to_Map, stamped_pose_Track_to_Track);
            std::cout<<"about to launch threads"<<std::endl;   

        // Launching the threads to handle the queues/passing of data
        image_representation_left_thread = std::thread(&SlamManager::process_image_representation_left_thread, this, std::ref(image_representation_left_running), std::ref(image_representation_left_cv));
        std::cout<<"ir left up"<<std::endl;
        image_representation_right_thread = std::thread(&SlamManager::process_image_representation_right_thread, this, std::ref(image_representation_right_running), std::ref(image_representation_right_cv));
        std::cout<<"ir right up"<<std::endl;
        mapping_thread = std::thread(&SlamManager::process_mapping_thread, this, std::ref(mapping_running), std::ref(mapping_cv));
        std::cout<<"maping up"<<std::endl;
        tracking_thread = std::thread(&SlamManager::process_tracking_thread, this, std::ref(tracking_running), std::ref(tracking_cv));
        std::cout<<"tracking up"<<std::endl;
    }

    void SlamManager::stopSlam()
    {
        std::cout<<"stopping slam"<<std::endl;
        // Setting the running booleans and setting status to terminate
        esvo2_core::setSystemStatus(esvo2_core::SystemStatus::TERMINATE);
        image_representation_left_running = false;
        image_representation_right_running = false;
        mapping_running = false;
        tracking_running = false;

        // joining all the threads
        std::cout<<"joining ir left"<<std::endl;
        image_representation_left_thread.join();

        std::cout<<"joining ir right"<<std::endl;
        image_representation_right_thread.join();

        std::cout<<"joining mapping"<<std::endl;
        mapping_thread.join();

        std::cout<<"joining tracking"<<std::endl;
        tracking_thread.join();

        std::cout<<"resetting yaml nodes"<<std::endl;

        // Resetting yaml nodes
        yaml_IR_Left_config.reset();
        yaml_IR_Right_config.reset();
        yaml_Track_config.reset();
        yaml_Map_config.reset();

        // Queues don't need to be reset because will be reassigned in operator= in startSlam.

        std::cout<<"sleeping"<<std::endl;
        // Stall to give time for the modules to terminate?
        std::this_thread::sleep_for(200ms);

        std::cout<<"resetting all the modules"<<std::endl;
        // Reset (deleting) all of the module's unique pointers
        std::cout<<"resetting ir left"<<std::endl;
        image_representation_left.reset(nullptr);
        std::cout<<"resetting ir right"<<std::endl;
        image_representation_right.reset(nullptr);
        std::cout<<"resetting mapping"<<std::endl;
        mapping.reset(nullptr);
        std::cout<<"resetting tracking"<<std::endl;
        tracking.reset(nullptr);
        
        std::cout<<"resetting remaining variables"<<std::endl;
        // Resetting remaining variables
        firstEventBatch = true;
        left_scrubber = nullptr;
        right_scrubber = nullptr;
        left_eventdata = nullptr;
        right_eventdata = nullptr;
        last_processed_event_time = 0;
    }

    void SlamManager::send_events()
    {
        // If SLAM is not running, just return
        if(!image_representation_left_running)
            return;
        std::cout<<"Sending events!"<<std::endl;
        sendEventsPerScrubber(*left_eventdata, *left_scrubber, true);
        sendEventsPerScrubber(*right_eventdata, *right_scrubber, false);
        std::cout<<"Sent events!"<<std::endl;
    }

    void SlamManager::sendEventsPerScrubber(EventData &event_data, Scrubber &scrubber, bool is_left)
    {
        // If SLAM is not running, just return
        if(!image_representation_left_running)
            return;

        const glm::vec2 frame_dims = event_data.get_camera_event_resolution();
        const std::size_t width = frame_dims[0];
        const std::size_t height = frame_dims[1];
        const std::size_t points_buffer_size = scrubber.get_points_buffer_size();
        const std::size_t current_lower_index = scrubber.get_current_lower_index();
        const auto &event_vector = event_data.get_evt_vector_ref();

        if(!event_vector.empty() && points_buffer_size > 0)
        {
            std::cout<<"event vector has size and scrub # points: "<<event_vector.size()<<" "<<points_buffer_size<<std::endl;
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
                // Do nanoseconds of (1000 * microseconds) so get higher precision
                std::chrono::nanoseconds end_duration(static_cast<long long>((data_ptr + (points_buffer_size - 1))->z * 1000));
                zero_absolute_timestamp = now - end_duration;
                firstEventBatch = false;
                std::cout<<"first time?"<<std::endl;
            }

            const double duration_threshold = 1.0/1000.0;
            double curr_lower_bound_timestamp = data_ptr->z;
            double upper_bound_timestamp = curr_lower_bound_timestamp + duration_threshold;

            esvo2_core::EventArray evtArray;
            evtArray.width = width;
            evtArray.height = height;

            std::cout<<"duration stuff: threshold, lower, upper: "<<duration_threshold<<" "<<curr_lower_bound_timestamp<<" "<<upper_bound_timestamp<<std::endl;

            for(std::size_t index = 0; index < points_buffer_size; index++)
            {
                // std::cout<<"index and pb size: "<<index<<" "<<points_buffer_size<<std::endl;
                // If no longer running, just return
                if(!image_representation_left_running)
                {
                    event_data.unlock_data_vectors();
                    return;
                }

                // Skip data if already processed it
                if(data_ptr[index].z <= last_processed_event_time)
                {
                    return;
                }

                // If reached the upper bound timestamp, can send the event array to the queues
                while(data_ptr[index].z >= upper_bound_timestamp)
                {
                    
                    // std::cout<<"reached upper bound timestamp: "<<curr_lower_bound_timestamp<<" "<<upper_bound_timestamp<<std::endl;
                    // Sending the Event Array to the queues
                    if(evtArray.events.size()!=0)
                    {
                    //    std::cout<<"about to send events to queues!"<<std::endl;
                       sendEventsToQueues(evtArray, is_left);
                       evtArray.events.clear();
                    //    std::cout<<"sent events to queue!"<<std::endl;
                    }
                    // Updating the lower/upper bounds
                    curr_lower_bound_timestamp = data_ptr[index].z;//upper_bound_timestamp;
                    upper_bound_timestamp = data_ptr[index].z + duration_threshold;//curr_lower_bound_timestamp + duration_threshold;
                }

                // Creating the event. Format is x->x, y->y, z->timestamp_relative, w->polarity
                esvo2_core::Event evt;
                evt.x = data_ptr[index].x;
                evt.y = data_ptr[index].y;
                evt.timestamp = std::chrono::nanoseconds(static_cast<long long>(data_ptr[index].z * 1000)) + zero_absolute_timestamp;
                evt.polarity = data_ptr[index].w;
                // Adding the event to the array
                evtArray.events.push_back(evt);
                if(index == points_buffer_size-1)
                {
                    last_processed_event_time = data_ptr[index].z;
                }
            }

            // std::cout<<"done with while loop in send events per scrubber"<<std::endl;
            // If evtArray is nonempty once finish for loop, send the events to the queues
            if(evtArray.events.size()!=0)
            {
                // std::cout<<"about to send events to queues!"<<" "<<is_left<<" "<<evtArray.events.size()<<std::endl;
                sendEventsToQueues(evtArray, is_left);
                // std::cout<<"sent events to queue!"<<std::endl;
            }
            event_data.unlock_data_vectors();
        }
    }

    void SlamManager::sendEventsToQueues(esvo2_core::EventArray& evtArray, bool is_left)
    {
        // std::cout<<"start of sendEventsToQueues function"<<std::endl;
        std::shared_ptr<esvo2_core::EventArray> final_evt_array = make_shared<esvo2_core::EventArray>();
        final_evt_array->width = evtArray.width;
        final_evt_array->height = evtArray.height;
        final_evt_array->events = std::move(evtArray.events);
        // std::cout<<"set up events array"<<std::endl;
        timePoint finalEventTimestamp = final_evt_array->events.at(final_evt_array->events.size()-1).timestamp;
        // std::cout<<"obtained final timestmap"<<std::endl;
        if(is_left)
        {
            // std::cout<<"here is what are adding:"<<std::endl;
            // std::cout<<esvo2_core::timePointToSec(finalEventTimestamp)<<std::endl;
            // std::cout<<final_evt_array->width<<" "<<final_evt_array->height<<std::endl;
            // for(auto& yep : final_evt_array->events)
            // {
                // std::cout<<yep.polarity<<" "<<esvo2_core::timePointToSec(yep.timestamp)<<" "<<yep.x<<" "<<yep.y<<std::endl;
            // }

            // std::cout<<"about to add to left"<<std::endl;
            event_left_To_IR.add(final_evt_array, finalEventTimestamp);
            event_left_To_Map.add(final_evt_array, finalEventTimestamp);
            // std::cout<<"succesfully added to left"<<std::endl;
        }
        else
        {
            // std::cout<<"about to add to right"<<std::endl;
            event_right_To_IR.add(final_evt_array, finalEventTimestamp);
            // std::cout<<"added to right"<<std::endl;
        }
    }

    void SlamManager::process_image_representation_left_thread(std::atomic<bool> &running, std::condition_variable& cv)
    {
        std::mutex mtx;
        unique_lock<mutex> lock(mtx);
        while(running)
        {
            cv.wait_for(lock, std::chrono::seconds(1));
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
                    // std::cout<<"doing IR left callback"<<std::endl;
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::EventArray>, timePoint> result = event_left_To_IR.getValue();
                    event_left_To_IR.unlock();
                    std::shared_ptr<esvo2_core::EventArray> evtArray = result.first;
                    image_representation_left->eventsCallback(evtArray);
                    // std::cout<<"did IR left callback"<<std::endl;
                }
            }
        }
    }

    void SlamManager::process_image_representation_right_thread(std::atomic<bool> &running, std::condition_variable& cv)
    {
        std::mutex mtx;
        unique_lock<mutex> lock(mtx);
        while(running)
        {
            cv.wait_for(lock, std::chrono::seconds(1));
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
                    // std::cout<<"sending IR right event callback"<<std::endl;
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::EventArray>, timePoint> result = event_right_To_IR.getValue();
                    event_right_To_IR.unlock();
                    std::shared_ptr<esvo2_core::EventArray> evtArray = result.first;
                    image_representation_right->eventsCallback(evtArray);
                    //  std::cout<<"sent IR right event callback"<<std::endl;
                }
            }
        }
    }

    void SlamManager::process_mapping_thread(std::atomic<bool> &running, std::condition_variable& cv)
    {
        std::mutex mtx;
        unique_lock<mutex> lock(mtx);
        while(running)
        {
            cv.wait_for(lock, std::chrono::seconds(1));
            // std::cout<<"mapping thread is checking through queues"<<std::endl;
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
                    // std::cout<<"mapping thread got multidata queue"<<std::endl;
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
                    // std::cout<<"mapping thread done processing multidata queue"<<std::endl;
                }


                // checking the stamped_pose_Track_to_Map
                stamped_pose_Track_to_Map.lock();
                if(stamped_pose_Track_to_Map.queueEmpty())
                {
                    stamped_pose_Track_to_Map.unlock();
                }
                else
                {
                    // std::cout<<"mapping thread got stamped pose"<<std::endl;
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::PoseStamped>, timePoint> result = stamped_pose_Track_to_Map.getValue();
                    stamped_pose_Track_to_Map.unlock();

                    std::shared_ptr<esvo2_core::PoseStamped> stamped_pose = result.first;
                    mapping->stampedPoseCallback(stamped_pose);
                    // std::cout<<"mapping thread done processing stamped pose"<<std::endl;
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
                        // std::cout<<"mapping thread got aa points"<<std::endl;
                        gotOne=true;
                        std::pair<std::shared_ptr<cv::Mat>, timePoint> result = AA_left_IR_to_Map.getValue();
                        AA_left_IR_to_Map.unlock();

                        esvo2_core::ImagePtr AA_left(result);
                        mapping->AACallback(AA_left);
                        // std::cout<<"mapping thread done processing aa points"<<std::endl;
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
                        // std::cout<<"mapping thread got events"<<std::endl;
                        gotOne=true;
                        std::pair<std::shared_ptr<esvo2_core::EventArray>, timePoint> result = event_left_To_Map.getValue();
                        event_left_To_Map.unlock();
                        std::shared_ptr<esvo2_core::EventArray> evtArray = result.first;
                        mapping->eventsCallback(evtArray);
                        // std::cout<<"mapping thread done processing events"<<std::endl;
                    }
                }
            }
            // std::cout<<"Mapping thread finished checking through queues"<<std::endl;
        }
    }

    void SlamManager::process_tracking_thread(std::atomic<bool> &running, std::condition_variable& cv)
    {
        std::mutex mtx;
        unique_lock<mutex> lock(mtx);
        while(running)
        {
            cv.wait_for(lock, std::chrono::seconds(1));
            
            // Get data until all queues are empty
            bool gotOne = true;
            // std::cout<<"Tracking thread about to check through queues"<<std::endl;
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
                    // std::cout<<"tracking thread got multidata queue"<<std::endl;
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
                    // std::cout<<"tracking thread done processing multidata queue"<<std::endl;
                }


                // checking the v_ba_bg_Map_to_Track
                v_ba_bg_Map_to_Track.lock();
                if(v_ba_bg_Map_to_Track.queueEmpty())
                {
                    v_ba_bg_Map_to_Track.unlock();
                }
                else
                {
                    // std::cout<<"tracking thread got babg bag"<<std::endl;
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::VBaBg>, timePoint> result = v_ba_bg_Map_to_Track.getValue();
                    v_ba_bg_Map_to_Track.unlock();

                    std::shared_ptr<esvo2_core::VBaBg> msg = result.first;
                    tracking->VBaBgCallback(msg);
                    // std::cout<<"tracking thread done processing babg bag"<<std::endl;
                }

                // checking the pointcloud_Map_to_Track
                pointcloud_Map_to_Track.lock();
                if(pointcloud_Map_to_Track.queueEmpty())
                {
                    pointcloud_Map_to_Track.unlock();
                }
                else
                {
                    // std::cout<<"tracking thread got pcl"<<std::endl;
                    gotOne=true;
                    std::pair<std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBL>>, timePoint> result = pointcloud_Map_to_Track.getValue();
                    pointcloud_Map_to_Track.unlock();

                    tracking->refMapCallback(result);
                    // std::cout<<"tracking thread done processing pcl"<<std::endl;
                }

                //stamped_pose_Track_to_Track
                stamped_pose_Track_to_Track.lock();
                if(stamped_pose_Track_to_Track.queueEmpty())
                {
                    stamped_pose_Track_to_Track.unlock();
                }
                else
                {
                    // std::cout<<"tracking thread got stamped pose"<<std::endl;
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::PoseStamped>, timePoint> result = stamped_pose_Track_to_Track.getValue();
                    stamped_pose_Track_to_Track.unlock();

                    std::shared_ptr<esvo2_core::PoseStamped> stamped_pose = result.first;
                    tracking->stampedPoseCallback(stamped_pose);
                    // std::cout<<"tracking thread done processing stamped pose"<<std::endl;
                }
            }
            // std::cout<<"Tracking thread finished checking through queues"<<std::endl;
        }
    }