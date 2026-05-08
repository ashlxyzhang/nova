#include "slam_manager.hh"
#include "unused/debug_log.hh"

#include <thread>
#include <atomic>
#include <condition_variable>

#include "util/pch.hh"
#include "data/EventData.hh"
#include "data/Scrubber.hh"

#include "image_representation/ImageRepresentation.h"
#include "esvo2_core/esvo2_Tracking.h"
#include "esvo2_core/esvo2_Mapping.h"
#include "esvo2_core/tools/types.h"
#include "data_passing.hh"
#include "multi_data_passing.hh"
#include "esvo2_core/tools/SystemStatus.h"

#include <opencv2/core/mat.hpp>
#include <pcl/point_types.h>

#include <iomanip>

namespace nova {

    SlamManager::SlamManager() {}

    // Spawns threads for all the modules
    void SlamManager::startSlam(StartSlamParameters params)
    {
        // Loading YAML config files
        yaml_IR_Left_config = YAML::LoadFile(IR_Left_yaml_path);
        yaml_IR_Right_config = YAML::LoadFile(IR_Right_yaml_path);
        yaml_Track_config = YAML::LoadFile(Tracking_yaml_path);
        yaml_Map_config = YAML::LoadFile(Mapping_yaml_path);

        // Setting the scrubbers
        this->left_scrubber = params.left_scrubber;
        this->right_scrubber = params.right_scrubber;
        this->left_eventdata = params.left_eventdata;
        this->right_eventdata = params.right_eventdata;

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

        // Making sure all modules have finished. Could probably use condition variables but whatever
        while(true)
        {
            if(image_representation_left != nullptr && !image_representation_left->getHasTerminated())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if(image_representation_right != nullptr && !image_representation_right->getHasTerminated())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if(mapping != nullptr && !mapping->getHasTerminated())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if(tracking != nullptr && !tracking->getHasTerminated())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            break;
        }

        // The module's constructors create and detach a new thread that manages their respective processes
        image_representation_left = std::make_unique<image_representation::ImageRepresentation>(
                image_representation_left_running, yaml_IR_Left_config, left_camera_yaml_path, 
                multi_to_Track, multi_to_Map, AA_left_IR_to_Map);
        image_representation_right = std::make_unique<image_representation::ImageRepresentation>(
                image_representation_right_running, yaml_IR_Right_config, right_camera_yaml_path, 
                multi_to_Track, multi_to_Map, AA_left_IR_to_Map);
        mapping = std::make_unique<esvo2_core::esvo2_Mapping>(
                mapping_running, yaml_Map_config, left_camera_yaml_path, right_camera_yaml_path,
                v_ba_bg_Map_to_Track, pointcloud_Map_to_Track);
        tracking = std::make_unique<esvo2_core::esvo2_Tracking>(
                tracking_running, yaml_Track_config, left_camera_yaml_path, right_camera_yaml_path,
                stamped_pose_Track_to_Map, stamped_pose_Track_to_Track);

        // Launching the threads to handle the queues/passing of data
        image_representation_left_thread = std::thread(&SlamManager::process_image_representation_left_thread, this, std::ref(image_representation_left_running), std::ref(image_representation_left_cv));
        image_representation_right_thread = std::thread(&SlamManager::process_image_representation_right_thread, this, std::ref(image_representation_right_running), std::ref(image_representation_right_cv));
        mapping_thread = std::thread(&SlamManager::process_mapping_thread, this, std::ref(mapping_running), std::ref(mapping_cv));
        tracking_thread = std::thread(&SlamManager::process_tracking_thread, this, std::ref(tracking_running), std::ref(tracking_cv));
    }

    void SlamManager::stopSlam()
    {
        // Setting the running booleans and setting status to terminate
        esvo2_core::setSystemStatus(esvo2_core::SystemStatus::TERMINATE);
        image_representation_left_running = false;
        image_representation_right_running = false;
        mapping_running = false;
        tracking_running = false;

        // joining all the threads
        if(image_representation_left_thread.joinable())
            image_representation_left_thread.join();

        if(image_representation_right_thread.joinable())
            image_representation_right_thread.join();

        if(mapping_thread.joinable())
            mapping_thread.join();

        if(tracking_thread.joinable())
            tracking_thread.join();

        // Resetting yaml nodes
        yaml_IR_Left_config.reset();
        yaml_IR_Right_config.reset();
        yaml_Track_config.reset();
        yaml_Map_config.reset();

        // Queues and modules don't need to be reset because will be reassigned in operator= in startSlam.

        // Resetting remaining variables
        left_scrubber = nullptr;
        right_scrubber = nullptr;
        left_eventdata = nullptr;
        right_eventdata = nullptr;
        last_processed_event_idx_left = 0;
        last_processed_event_idx_right = 0;
    }

    void SlamManager::set_config_file(std::string file_path)
    {
        switch(current_config_file_type)
        {
            case(SlamConfigFiles::IR_Left):
            {
                IR_Left_yaml_path = file_path;
                break;
            }
            case(SlamConfigFiles::IR_Right):
            {
                IR_Right_yaml_path = file_path;
                break;
            }
            case(SlamConfigFiles::Tracking):
            {
                Tracking_yaml_path = file_path;
                break;
            }
            case(SlamConfigFiles::Mapping):
            {
                Mapping_yaml_path = file_path;
                break;
            }
            case(SlamConfigFiles::Camera_Left):
            {
                left_camera_yaml_path = file_path;
                break;
            }
            case(SlamConfigFiles::Camera_Right):
            {
                right_camera_yaml_path = file_path;
                break;
            }
        }
    }

    std::string SlamManager::get_config_file_path(SlamConfigFiles type)
    {
        switch(type)
        {
            case(SlamConfigFiles::IR_Left):
                return IR_Left_yaml_path;
            case(SlamConfigFiles::IR_Right):
                return IR_Right_yaml_path;
            case(SlamConfigFiles::Tracking):
                return Tracking_yaml_path;
            case(SlamConfigFiles::Mapping):
                return Mapping_yaml_path;
            case(SlamConfigFiles::Camera_Left):
                return left_camera_yaml_path;
            case(SlamConfigFiles::Camera_Right):
                return right_camera_yaml_path;
        }
        return "";
    }

    void SlamManager::send_events()
    {
        // If SLAM is not running, just return
        if(!image_representation_left_running)
            return;
        sendEventsPerScrubber(*left_eventdata, *left_scrubber, true);
        sendEventsPerScrubber(*right_eventdata, *right_scrubber, false);
    }

    void SlamManager::sendEventsPerScrubber(EventData &event_data, Scrubber &scrubber, bool is_left)
    {
        // If SLAM is not running, just return
        if(!image_representation_left_running)
            return;
        
        std::size_t& last_processed_event_idx = is_left ? last_processed_event_idx_left : last_processed_event_idx_right;
        const glm::vec2 frame_dims = event_data.get_camera_event_resolution();
        const std::size_t width = frame_dims[0];
        const std::size_t height = frame_dims[1];
        const std::size_t points_buffer_size = scrubber.get_points_buffer_size();
        const std::size_t current_lower_index = scrubber.get_current_lower_index();
        const auto &event_vector = event_data.get_evt_vector_ref();

        if(!event_vector.empty() && points_buffer_size > 0)
        {
            const glm::vec4* data_ptr = event_data.get_evt_vector_ref().data() + current_lower_index;
            event_data.lock_data_vectors();
            const double duration_threshold = 1.0/1000.0;
            double curr_lower_bound_timestamp = data_ptr->z;
            double upper_bound_timestamp = curr_lower_bound_timestamp + duration_threshold;

            esvo2_core::EventArray evtArray;
            evtArray.width = width;
            evtArray.height = height;

            int num_skipped = 0;
            int num_did = 0;
            // Skip data if already processed it
            int start_index = 0;
            if(last_processed_event_idx >= current_lower_index)
            {
                start_index = last_processed_event_idx - current_lower_index + 1;
            }
            for(std::size_t index = start_index; index < points_buffer_size; index++)
            {
                // If no longer running, just return
                if(!image_representation_left_running)
                {
                    event_data.unlock_data_vectors();
                    return;
                }

                num_did++;

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
                    curr_lower_bound_timestamp = data_ptr[index].z;//upper_bound_timestamp;
                    upper_bound_timestamp = data_ptr[index].z + duration_threshold;//curr_lower_bound_timestamp + duration_threshold;
                }

                // Creating the event. Format is x->x, y->y, z->timestamp_relative, w->polarity
                esvo2_core::Event evt;
                evt.x = data_ptr[index].x;
                evt.y = data_ptr[index].y;
                // https://docs.inivation.com/software/introduction.html: "timestamp represents the time of the start of exposure of the 
                // frame. It is represented as a Unix Timestamp in **microseconds**. Type: int64"
                // Do nanoseconds of (microseconds * 1000) so get higher precision
                evt.timestamp = std::chrono::nanoseconds(static_cast<long long>(data_ptr[index].z * 1000)) + zero_absolute_timestamp;
                evt.polarity = data_ptr[index].w;
                // Adding the event to the array
                evtArray.events.push_back(evt);
            }

            if(points_buffer_size != 0)
                last_processed_event_idx = std::max(last_processed_event_idx, current_lower_index + points_buffer_size - 1);
            // If evtArray is nonempty once finish for loop, send the events to the queues
            if(evtArray.events.size()!=0)
            {
                sendEventsToQueues(evtArray, is_left);
            }
            event_data.unlock_data_vectors();
        }
    }

    void SlamManager::sendEventsToQueues(esvo2_core::EventArray& evtArray, bool is_left)
    {
        std::shared_ptr<esvo2_core::EventArray> final_evt_array = make_shared<esvo2_core::EventArray>();
        final_evt_array->width = evtArray.width;
        final_evt_array->height = evtArray.height;
        final_evt_array->events = std::move(evtArray.events);
        timePoint finalEventTimestamp = final_evt_array->events.at(final_evt_array->events.size()-1).timestamp;
        if(is_left)
        {
            event_left_To_IR.add(final_evt_array, finalEventTimestamp);
            event_left_To_Map.add(final_evt_array, finalEventTimestamp);
        }
        else
        {
            event_right_To_IR.add(final_evt_array, finalEventTimestamp);
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
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::EventArray>, timePoint> result = event_left_To_IR.getValue();
                    event_left_To_IR.unlock();
                    std::shared_ptr<esvo2_core::EventArray> evtArray = result.first;
                    image_representation_left->eventsCallback(evtArray);
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
                    gotOne=true;
                    std::pair<std::shared_ptr<esvo2_core::EventArray>, timePoint> result = event_right_To_IR.getValue();
                    event_right_To_IR.unlock();
                    std::shared_ptr<esvo2_core::EventArray> evtArray = result.first;
                    image_representation_right->eventsCallback(evtArray);
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

    void SlamManager::process_tracking_thread(std::atomic<bool> &running, std::condition_variable& cv)
    {
        std::mutex mtx;
        unique_lock<mutex> lock(mtx);
        while(running)
        {
            cv.wait_for(lock, std::chrono::seconds(1));
            
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

                // checking the pointcloud_Map_to_Track. Needs to come after stamped pose callback because requires the 
                //    transform from stamped pose to be added.
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
            }
        }
    }
} // namespace nova