#pragma once
#ifndef DATA_ACQUISITION_HH
#define DATA_ACQUISITION_HH

#include "DataWriter.hh"
#include "EventData.hh"
#include "ParameterStore.hh"
#include <dv-processing/io/mono_camera_recording.hpp>

class DataAcquisition
{

    private:
        std::shared_ptr<dv::io::MonoCameraRecording> data_reader_ptr;
        int32_t camera_width;
        int32_t camera_height;

        std::mutex acq_lock; // For thread safety
        // From old NOVA source code
        // to get random float from 0.0 to 1.0
        float randFloat()
        {
            return static_cast<float>(rand()) / RAND_MAX;
        };

    public:
        DataAcquisition() : data_reader_ptr{}, camera_width{}, camera_height{}, acq_lock{}
        {
        }

        /**
         * Loads file to read. Initializes internal reader with filename.
         * @param file_name the name of the data file to read.
         * @return false if failed to init reader, true otherwise.
         */
        bool init_reader(std::string file_name)
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            // FROM OLD NOVA source code
            // Verify provided file is aedat4
            size_t extension_pos = file_name.find_last_of('.');
            if (extension_pos == std::string::npos || file_name.substr(extension_pos) != ".aedat4")
            {
                std::cerr << "ERROR: File extension is not .aedat4" << std::endl;
                acq_lock_ul.unlock();
                return false;
            }

            data_reader_ptr = std::make_shared<dv::io::MonoCameraRecording>(file_name);
            camera_width = data_reader_ptr->getEventResolution().value().width;
            camera_height = data_reader_ptr->getEventResolution().value().height;
            acq_lock_ul.unlock();
            return true;
        }

        /**
         * Gives camera resolution to event data.
         * @param evt_data Event data to give camera resolution to.
         */
        void get_camera_resolution(EventData &evt_data)
        {
            evt_data.set_camera_resolution(camera_width, camera_height);
        }

        /**
         * For static loading, gets all event data from a file and populates evt_data with them.
         * @param evt_data EventData object to populate with event/frame data
         * @param param_store ParameterStore object with data from GUI.
         */
        void get_all_evt_data(EventData &evt_data, ParameterStore &param_store, DataWriter &data_writer)
        {
            while (get_batch_evt_data(evt_data, param_store, data_writer))
            {
            }
        }

        /**
         * For static loading, gets all frame data from a file and populates evt_data with them.
         * @param evt_data EventData object to populate with event/frame data
         * @param param_store ParameterStore object with data from GUI.
         */
        void get_all_frame_data(EventData &evt_data, ParameterStore &param_store, DataWriter &data_writer)
        {
            while (get_batch_frame_data(evt_data, param_store, data_writer))
            {
            }
        }

        /**
         * For dynamic loading (streaming), gets a batch of event data.
         * @param evt_data EventData object to populate with event/frame data
         * @param param_store ParameterStore object with data from GUI.
         * @return true if data was read, false otherwise.
         */
        bool get_batch_evt_data(EventData &evt_data, ParameterStore &param_store, DataWriter &data_writer)
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            // If reader is not properly initialized, return immediately
            if (!data_reader_ptr)
            {
                acq_lock_ul.unlock();
                return false;
            }
            bool data_read = false;

            // Discard threshold
            float threshold{1.0f / param_store.get<float>("event_discard_odds")};

            // https://dv-processing.inivation.com/rel_1_7/reading_data.html#read-events-from-a-file
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
                    if (param_store.get<bool>("stream_save"))
                    {
                        data_writer.add_event_store(event_store);
                    }
                }
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
                    if (param_store.get<bool>("stream_save"))
                    {
                        dv::Frame frame_datum(frame_data->timestamp, frame_data->image);
                        data_writer.add_frame_data(frame_datum);
                    }
                }
            }
            acq_lock_ul.unlock();
            return data_read;
        }

        /**
         * @brief Returns camera width.
         * @return camera_width
         */
        int32_t get_camera_width()
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            int32_t ret_camera_width{camera_width};
            acq_lock_ul.unlock();
            return ret_camera_width;
        }

        /**
         * @brief Returns camera height.
         * @return camera_height
         */
        int32_t get_camera_height()
        {
            std::unique_lock<std::mutex> acq_lock_ul{acq_lock};
            int32_t ret_camera_height{camera_height};
            acq_lock_ul.unlock();
            return ret_camera_height;
        }
};

#endif // DATA_ACQUISITION_HH
