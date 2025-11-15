#pragma once
#ifndef DATA_ACQUISITION_HH
#define DATA_ACQUISITION_HH

#include "DataWriter.hh"
#include "EventData.hh"
#include "ParameterStore.hh"
#include <dv-processing/io/camera/discovery.hpp>
#include <dv-processing/io/camera/usb_device.hpp>
#include <dv-processing/io/mono_camera_recording.hpp>
#include <vector>

/**
 * @brief This class provides functions for getting event/frame data
 *        from an aedat4 file or an event imager.
 *
 */
class DataAcquisition
{

    private:
        // For storing currently detected cameras
        std::vector<dv::io::camera::USBDevice::DeviceDescriptor> scanned_cameras;

        // Super class data reader object provided by DV-Processing library.
        // Can get camera and file data.
        std::unique_ptr<dv::io::InputBase> data_reader_ptr;

        int32_t camera_event_width;
        int32_t camera_event_height;

        int32_t camera_frame_width;
        int32_t camera_frame_height;

        std::mutex acq_lock; // For thread safety

        /**
         * @brief From old NOVA source code
         *        to get random float from 0.0 to 1.0
         * @return random float from 0.0 to 1.0
         */

        float randFloat()
        {
            return static_cast<float>(rand()) / RAND_MAX;
        };

    public:
        /**
         * @brief Constructor, zero initializes all variables
         */
        DataAcquisition()
            : data_reader_ptr{}, camera_event_width{}, camera_event_height{}, camera_frame_width{},
              camera_frame_height{}, acq_lock{}
        {
        }

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
         * Clears every member variable
         */
        void clear()
        {
            clear_reader();
            scanned_cameras.clear();
        }

        /**
         * Scan for cameras and populate parameter store with GUI values to choose from.
         * @param param_store Parameter store to populate.
         */
        void discover_cameras(ParameterStore &param_store)
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            // Remove previously scanned cameras
            scanned_cameras.clear();

            // Discover available cameras
            // https://dv-processing.inivation.com/master/reading_data.html

            const auto discovered_cameras{dv::io::camera::discover()};

            std::vector<std::string> cameras_vec{};

            for (const auto &camera : discovered_cameras)
            {
                std::stringstream str_stream{};
                scanned_cameras.push_back(camera);
                // To ensure string conversion works
                str_stream << "Model: " << camera.cameraModel << " ";
                str_stream << "Serial Number: " << camera.serialNumber << "\0";
                cameras_vec.push_back(str_stream.str());
            }
            param_store.add("discovered_cameras", cameras_vec);

            acq_lock_ul.unlock();
        }

        /**
         * Loads camera to read. Initializes internal reader with camera.
         * @param camera_index Index of camera in scanned_cameras vector to stream from.
         * @param param_store param_store ParameterStore necessary for storing error messages in cases of failure.
         * @return false if failed to init reader, true otherwise.
         */
        bool init_camera_reader(int32_t camera_index, ParameterStore &param_store)
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};

            // Sanity check
            if (scanned_cameras.empty() || camera_index < 0 || camera_index >= scanned_cameras.size())
            {
                acq_lock_ul.unlock();
                return false;
            }

            try
            {
                data_reader_ptr = std::move(
                    dv::io::camera::open(scanned_cameras[camera_index])); // Specify move semantics for unique pointer
            }
            catch (...)
            {
                std::string pop_up_err_str{"Something went wrong with the camera for reading!"};
                param_store.add("pop_up_err_str", pop_up_err_str);
                acq_lock_ul.unlock();
                return false;
            }

            if (data_reader_ptr->isEventStreamAvailable())
            {
                auto evt_resolution = data_reader_ptr->getEventResolution();
                if (evt_resolution.has_value())
                {
                    camera_event_width = evt_resolution.value().width;
                    camera_event_height = evt_resolution.value().height;
                }
            }
            if (data_reader_ptr->isFrameStreamAvailable())
            {
                auto frame_resolution = data_reader_ptr->getFrameResolution();
                if (frame_resolution.has_value())
                {
                    camera_frame_width = frame_resolution.value().width;
                    camera_frame_height = frame_resolution.value().height;
                }
            }
            acq_lock_ul.unlock();
            return true;
        }

        /**
         * Loads file to read. Initializes internal reader with filename.
         * @param file_name the name of the data file to read.
         * @param param_store ParameterStore necessary for storing error messages in cases of failure.
         * @return false if failed to init reader, true otherwise.
         */
        bool init_file_reader(std::string file_name, ParameterStore &param_store)
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            // FROM OLD NOVA source code
            // Verify provided file is aedat4
            size_t extension_pos = file_name.find_last_of('.');
            if (extension_pos == std::string::npos || file_name.substr(extension_pos) != ".aedat4")
            {
                std::string pop_up_err_str{"File extension is not .aedat4!"};
                param_store.add("pop_up_err_str", pop_up_err_str);
                acq_lock_ul.unlock();
                return false;
            }

            try
            {
                data_reader_ptr = std::make_unique<dv::io::MonoCameraRecording>(file_name);
            }
            catch (...)
            {
                std::string pop_up_err_str{"Something went wrong while initializing file for reading!"};
                param_store.add("pop_up_err_str", pop_up_err_str);
                acq_lock_ul.unlock();
                return false;
            }
            if (data_reader_ptr->isEventStreamAvailable())
            {
                auto evt_resolution = data_reader_ptr->getEventResolution();
                if (evt_resolution.has_value())
                {
                    camera_event_width = evt_resolution.value().width;
                    camera_event_height = evt_resolution.value().height;
                }
            }
            if (data_reader_ptr->isFrameStreamAvailable())
            {
                auto frame_resolution = data_reader_ptr->getFrameResolution();
                if (frame_resolution.has_value())
                {
                    camera_frame_width = frame_resolution.value().width;
                    camera_frame_height = frame_resolution.value().height;
                }
            }
            acq_lock_ul.unlock();
            return true;
        }

        /**
         * Gives event camera resolution to event data.
         * @param evt_data EventData object to give camera resolution to.
         */
        void get_camera_event_resolution(EventData &evt_data)
        {
            evt_data.set_camera_event_resolution(camera_event_width, camera_event_height);
        }

        /**
         * Gives frame camera resolution to event data.
         * @param evt_data EventData object to give camera resolution to.
         */
        void get_camera_frame_resolution(EventData &evt_data)
        {
            evt_data.set_camera_frame_resolution(camera_frame_width, camera_frame_height);
        }

        /**
         * For dynamic loading (streaming), gets a batch of event data.
         * @param evt_data EventData object to populate with event/frame data
         * @param param_store ParameterStore object with data from GUI.
         * @return true if data was read, false otherwise.
         */
        bool get_batch_evt_data(EventData &evt_data, ParameterStore &param_store, DataWriter &data_writer,
                                float event_discard_odds)
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            // If reader is not properly initialized, return immediately
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

            // Discard threshold
            float threshold{1.0f / event_discard_odds};

            // https://dv-processing.inivation.com/rel_1_7/reading_data.html#read-events-from-a-file

            try
            {
                if (data_reader_ptr->isEventStreamAvailable() && data_reader_ptr->isRunning("events"))
                {
                    if (const auto events = data_reader_ptr->getNextEventBatch(); events.has_value())
                    {
                        // In case of persistent storage
                        // https://dv-processing.inivation.com/master/event_store.html
                        dv::EventStore event_store{};
                        for (auto &evt : events.value())
                        {
                            if (randFloat() > threshold) // Random discard
                            {
                                continue; // Discard
                            }
                            EventData::EventDatum evt_datum{
                                .x = evt.x(), .y = evt.y(), .timestamp = evt.timestamp(), .polarity = evt.polarity()};

                            evt_data.write_evt_data(evt_datum);
                            data_read = true;

                            event_store.emplace_back(evt.timestamp(), evt.x(), evt.y(), evt.polarity());
                        }

                        // Add to queue for persistent storage in case of persistent storage
                        if (data_writer.get_writing_event_data())
                        {
                            data_writer.add_event_store(event_store);
                        }
                    }
                }
            }
            catch (...)
            {
                std::string pop_up_err_str{"Something went wrong with reading event data!"};
                param_store.add("pop_up_err_str", pop_up_err_str);
                acq_lock_ul.unlock();
                return false;
            }
            acq_lock_ul.unlock();
            return data_read;
        }

        /**
         * For dynamic loading (streaming), gets a batch of frame data.
         * @param evt_data EventData object to populate with event/frame data
         * @param param_store ParameterStore object with data from GUI.
         * @return true if data was read, false otherwise.
         */
        bool get_batch_frame_data(EventData &evt_data, ParameterStore &param_store, DataWriter &data_writer)
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            // If reader is not initialized, return immediately
            if (!data_reader_ptr)
            {
                acq_lock_ul.unlock();
                return false;
            }
            bool data_read = false;

            // https://dv-processing.inivation.com/rel_1_7/reading_data.html#read-frames-from-a-file

            try
            {
                if (data_reader_ptr->isFrameStreamAvailable() && data_reader_ptr->isRunning("frames"))
                {
                    if (const auto frame_data = data_reader_ptr->getNextFrame(); frame_data.has_value())
                    {
                        cv::Mat out;
                        // Convert frame data from BGR to RGB
                        cv::cvtColor(frame_data->image, out, cv::COLOR_BGR2RGB);

                        // CLone to ensure tightly packed frame bytes
                        EventData::FrameDatum frame_datum{.frameData = out.clone(), .timestamp = frame_data->timestamp};

                        evt_data.write_frame_data(frame_datum);
                        data_read = true;

                        // If saving stream, add to queue to write
                        if (data_writer.get_writing_frame_data())
                        {
                            dv::Frame frame_datum(frame_data->timestamp, frame_data->image);
                            data_writer.add_frame_data(frame_datum);
                        }
                    }
                }
            }
            catch (...)
            {
                std::string pop_up_err_str{"Something went wrong with reading frame data!"};
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
