#pragma once
#ifndef DATA_ACQUISITION_HH
#define DATA_ACQUISITION_HH

#include "DataWriter.hh"
#include "EventData.hh"

#include "IEventReader.hh"
#include "DVEventReader.hh"
#include "MetavisionEventReader.hh"

#include <dv-processing/io/camera/discovery.hpp>
#include <dv-processing/io/camera/usb_device.hpp>
#include <opencv2/imgproc.hpp>

#include <vector>

/**
 * @brief This class provides functions for getting event/frame data
 *        from an event camera file or a live event imager.
 *
 * File format routing:
 *   .aedat4          -> DVEventReader  (dv-processing)
 *   .raw / .dat      -> MetavisionEventReader  (OpenEB)
 */
class DataAcquisition
{

    private:
        // For storing currently detected cameras
        std::vector<dv::io::camera::USBDevice::DeviceDescriptor> scanned_cameras;

        // Backend-agnostic event reader; created by init_file_reader / init_camera_reader
        std::unique_ptr<IEventReader> data_reader_ptr;

        int32_t camera_event_width;
        int32_t camera_event_height;

        int32_t camera_frame_width;
        int32_t camera_frame_height;

        float randFloat()
        {
            return static_cast<float>(rand()) / RAND_MAX;
        };

    public:
        
        enum class PROGRAM_STATE : uint8_t
        {
            IDLE = 0,         
            FILE_STREAM = 2,  
            CAMERA_STREAM = 3 
        };
        

        std::shared_mutex mutex;

        PROGRAM_STATE program_state = PROGRAM_STATE::IDLE;

        // Man wtf was phase 2 doing...
        bool camera_changed = false;
        bool stream_file_changed = false;       
        bool stream_paused = false;
        bool camera_stream_paused = false;
        
        int32_t camera_index = -1; // -1 if no camera currently selected
        std::vector<std::string> discovered_camera_names;
       
         std::string stream_file_name = "";

        float event_discard_odds = 1.0f; // 1.0 keeps all, 0 keeps none 


        DataAcquisition():  data_reader_ptr{}, camera_event_width{}, camera_event_height{}, camera_frame_width{},
                            camera_frame_height{} {}

        /**
         * @brief Clears member variables pertaining to reader, including reseting the data reader.
         */
        void clear_reader()
        {
            data_reader_ptr.reset();

            camera_event_width = 0;
            camera_event_height = 0;

            camera_frame_width = 0;
            camera_frame_height = 0;
        }

        /**
         * @brief Clears every member variable
         */
        void clear()
        {
            clear_reader();
            scanned_cameras.clear();
        }

        /**
         * @brief Scan for available USB cameras load camera names and their pointers into discovered_camera_names and 
         * scanned_cameras respectively
         */
        void discover_cameras()
        {
            std::unique_lock da_read_write_lock(mutex);
            
            scanned_cameras.clear();
            discovered_camera_names.clear();
            
            const auto discovered_cameras{dv::io::camera::discover()};
            for (const auto &camera : discovered_cameras)
            {
                scanned_cameras.push_back(camera);
                
                std::stringstream str_stream;
                str_stream << "Model: " << camera.cameraModel << " ";
                str_stream << "Serial Number: " << camera.serialNumber << "\0";
                discovered_camera_names.push_back(str_stream.str());
            }
        }

        /**
         * @brief Loads camera to read. Initializes internal reader with camera.
         * @param camera_index Index of camera in scanned_cameras vector to stream from.
         * @param param_store ParameterStore necessary for storing error messages in cases of failure.
         * @return false if failed to init reader, true otherwise.
         */
        bool init_camera_reader()
        {
            std::unique_lock da_read_write_lock(mutex);

            // Ensure valid parameters
            if (scanned_cameras.empty() || camera_index < 0 || camera_index >= scanned_cameras.size()) return false;



            try
            {
                data_reader_ptr = std::make_unique<DVEventReader>(dv::io::camera::open(scanned_cameras[camera_index]));
            }
            catch (const std::exception &e)
            {
                std::string pop_up_err_str{"Camera reader error: "};
                pop_up_err_str += e.what();
                param_store.add("pop_up_err_str", pop_up_err_str);

                
                return false;
            }

            if (data_reader_ptr->isEventStreamAvailable())
            {
                auto evt_resolution = data_reader_ptr->getEventResolution();
                if (evt_resolution.has_value())
                {
                    camera_event_width  = evt_resolution->width;
                    camera_event_height = evt_resolution->height;
                }
            }
            if (data_reader_ptr->isFrameStreamAvailable())
            {
                auto frame_resolution = data_reader_ptr->getFrameResolution();
                if (frame_resolution.has_value())
                {
                    camera_frame_width  = frame_resolution->width;
                    camera_frame_height = frame_resolution->height;
                }
            }
            acq_lock_ul.unlock();
            return true;
        }

        /**
         * @brief Loads file to read. Initializes internal reader based on file extension:
         *          .aedat4        -> dv-processing backend
         *          .raw / .dat    -> OpenEB/Metavision backend
         * @param file_name the name of the data file to read.
         * @param param_store ParameterStore necessary for storing error messages in cases of failure.
         * @return false if failed to init reader, true otherwise.
         */
        bool init_file_reader(std::string file_name, ParameterStore &param_store)
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};

            size_t extension_pos = file_name.find_last_of('.');
            if (extension_pos == std::string::npos)
            {
                std::string pop_up_err_str{"File has no extension!"};
                param_store.add("pop_up_err_str", pop_up_err_str);
                acq_lock_ul.unlock();
                return false;
            }

            std::string ext = file_name.substr(extension_pos);

            if (ext == ".aedat4")
            {
                try
                {
                    data_reader_ptr = std::make_unique<DVEventReader>(
                        std::make_unique<dv::io::MonoCameraRecording>(file_name));
                }
                catch (const std::exception &e)
                {
                    std::string pop_up_err_str{"aedat4 reader error: "};
                    pop_up_err_str += e.what();
                    param_store.add("pop_up_err_str", pop_up_err_str);
                    acq_lock_ul.unlock();
                    return false;
                }
            }
            else if (ext == ".raw" || ext == ".dat")
            {
                try
                {
                    data_reader_ptr = std::make_unique<MetavisionEventReader>(file_name);
                }
                catch (const std::exception &e)
                {
                    std::string pop_up_err_str{"Metavision reader error: "};
                    pop_up_err_str += e.what();
                    param_store.add("pop_up_err_str", pop_up_err_str);
                    acq_lock_ul.unlock();
                    return false;
                }
            }
            else
            {
                std::string pop_up_err_str{"Unsupported file extension. Supported formats: .aedat4, .raw, .dat"};
                param_store.add("pop_up_err_str", pop_up_err_str);
                acq_lock_ul.unlock();
                return false;
            }

            if (data_reader_ptr->isEventStreamAvailable())
            {
                auto evt_resolution = data_reader_ptr->getEventResolution();
                if (evt_resolution.has_value())
                {
                    camera_event_width  = evt_resolution->width;
                    camera_event_height = evt_resolution->height;
                }
            }
            if (data_reader_ptr->isFrameStreamAvailable())
            {
                auto frame_resolution = data_reader_ptr->getFrameResolution();
                if (frame_resolution.has_value())
                {
                    camera_frame_width  = frame_resolution->width;
                    camera_frame_height = frame_resolution->height;
                }
            }
            acq_lock_ul.unlock();
            return true;
        }

        /**
         * @brief Gives event camera resolution to event data.
         * @param evt_data EventData object to give camera resolution to.
         */
        void get_camera_event_resolution(EventData &evt_data)
        {
            evt_data.set_camera_event_resolution(camera_event_width, camera_event_height);
        }

        /**
         * @brief Gives frame camera resolution to event data.
         * @param evt_data EventData object to give camera resolution to.
         */
        void get_camera_frame_resolution(EventData &evt_data)
        {
            evt_data.set_camera_frame_resolution(camera_frame_width, camera_frame_height);
        }

        /**
         * @brief For dynamic loading (streaming), gets a batch of event data.
         * @param evt_data EventData object to populate with event data.
         * @param param_store ParameterStore object with data from GUI.
         * @param data_writer DataWriter for optional persistent storage.
         * @param event_discard_odds Odds of keeping each event (1.0 = keep all).
         * @return true if any data was read, false otherwise.
         */
        bool get_batch_evt_data(EventData &evt_data, ParameterStore &param_store, DataWriter &data_writer,
                                float event_discard_odds)
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            if (!data_reader_ptr)
            {
                acq_lock_ul.unlock();
                return false;
            }
            bool data_read = false;

            if (event_discard_odds < 0.00001)
            {
                std::string pop_up_err_str{"Event Discard Odds are too low!"};
                param_store.add("pop_up_err_str", pop_up_err_str);
                acq_lock_ul.unlock();
                return false;
            }

            float threshold{1.0f / event_discard_odds};

            try
            {
                if (data_reader_ptr->isEventStreamAvailable() && data_reader_ptr->isEventsRunning())
                {
                    if (const auto events = data_reader_ptr->getNextEventBatch(); events.has_value())
                    {
                        dv::EventStore event_store{};
                        for (const auto &evt : *events)
                        {
                            if (randFloat() > threshold)
                                continue;

                            evt_data.write_evt_data(evt);
                            data_read = true;

                            event_store.emplace_back(evt.timestamp, evt.x, evt.y, evt.polarity);
                        }

                        if (data_writer.get_writing_event_data())
                        {
                            data_writer.add_event_store(event_store);
                        }
                    }
                }
            }
            catch (const std::exception &e)
            {
                std::string pop_up_err_str{"Event read error: "};
                pop_up_err_str += e.what();
                param_store.add("pop_up_err_str", pop_up_err_str);
                acq_lock_ul.unlock();
                return false;
            }
            acq_lock_ul.unlock();
            return data_read;
        }

        /**
         * @brief For dynamic loading (streaming), gets a batch of frame data.
         * @param evt_data EventData object to populate with frame data.
         * @param param_store ParameterStore object with data from GUI.
         * @param data_writer DataWriter for optional persistent storage.
         * @return true if data was read, false otherwise.
         */
        bool get_batch_frame_data(EventData &evt_data, ParameterStore &param_store, DataWriter &data_writer)
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            if (!data_reader_ptr)
            {
                acq_lock_ul.unlock();
                return false;
            }
            bool data_read = false;

            try
            {
                if (data_reader_ptr->isFrameStreamAvailable() && data_reader_ptr->isFramesRunning())
                {
                    if (const auto frame = data_reader_ptr->getNextFrame(); frame.has_value())
                    {
                        // Convert BGR (native) to RGB for display
                        cv::Mat rgb;
                        cv::cvtColor(frame->frameData, rgb, cv::COLOR_BGR2RGB);
                        EventData::FrameDatum display_frame{.frameData = rgb.clone(),
                                                            .timestamp = frame->timestamp};
                        evt_data.write_frame_data(display_frame);
                        data_read = true;

                        // Write original BGR image to file
                        if (data_writer.get_writing_frame_data())
                        {
                            dv::Frame dv_frame(frame->timestamp, frame->frameData);
                            data_writer.add_frame_data(dv_frame);
                        }
                    }
                }
            }
            catch (const std::exception &e)
            {
                std::string pop_up_err_str{"Frame read error: "};
                pop_up_err_str += e.what();
                param_store.add("pop_up_err_str", pop_up_err_str);
                acq_lock_ul.unlock();
                return false;
            }
            acq_lock_ul.unlock();
            return data_read;
        }

        /**
         * @brief Returns event camera width.
         * @return event_camera_width
         */
        int32_t get_camera_event_width()
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            int32_t ret_camera_width{camera_event_width};
            acq_lock_ul.unlock();
            return ret_camera_width;
        }

        /**
         * @brief Returns frame camera width.
         * @return frame_camera_width
         */
        int32_t get_camera_frame_width()
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            int32_t ret_camera_width{camera_frame_width};
            acq_lock_ul.unlock();
            return ret_camera_width;
        }

        /**
         * @brief Returns event camera height.
         * @return event_camera_height
         */
        int32_t get_camera_event_height()
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            int32_t ret_camera_height{camera_event_height};
            acq_lock_ul.unlock();
            return ret_camera_height;
        }

        /**
         * @brief Returns frame camera height.
         * @return frame_camera_height
         */
        int32_t get_camera_frame_height()
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            int32_t ret_camera_height{camera_frame_height};
            acq_lock_ul.unlock();
            return ret_camera_height;
        }
};

#endif // DATA_ACQUISITION_HH
