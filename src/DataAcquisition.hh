#pragma once
#ifndef DATA_ACQUISITION_HH
#define DATA_ACQUISITION_HH

#include "EventData.hh"
#include "ParameterStore.hh"
#include <dv-processing/io/mono_camera_recording.hpp>

class DataAcquisition
{
    public:
        DataAcquisition() : data_reader_ptr{}, camera_width{}, camera_height{}
        {
        }

        /**
         * Loads file to read. Initializes internal reader with filename.
         * @param file_name the name of the data file to read.
         */
        void load_file(std::string file_name)
        {
            data_reader_ptr = std::make_shared<dv::io::MonoCameraRecording>(file_name);
            camera_width = data_reader_ptr->getEventResolution().value().width;
            camera_height = data_reader_ptr->getEventResolution().value().height;
        }

        /**
         * For static loading, gets all event data from a file and populates evt_data with them.
         * @param evt_data EventData object to populate with event/frame data
         * @param param_store ParameterStore object with data from GUI.
         */
        void get_all_evt_data(EventData &evt_data, ParameterStore &param_store)
        {
            while (get_evt_batch_data(evt_data, param_store))
            {
            }
        }

        /**
         * For static loading, gets all frame data from a file and populates evt_data with them.
         * @param evt_data EventData object to populate with event/frame data
         * @param param_store ParameterStore object with data from GUI.
         */
        void get_all_frame_data(EventData &evt_data, ParameterStore &param_store)
        {
            while (get_frame_batch_data(evt_data, param_store))
            {
            }
        }

        /**
         * For dynamic loading (streaming), gets a batch of event data.
         * @param evt_data EventData object to populate with event/frame data
         * @param param_store ParameterStore object with data from GUI.
         * @return true if data was read, false otherwise.
         */
        bool get_evt_batch_data(EventData &evt_data, ParameterStore &param_store)
        {
            // If reader is not properly initialized, return immediately
            if (!data_reader_ptr)
            {
                return false;
            }
            bool data_read = false;

            // Discard threshold
            float threshold{1.0f / param_store.get<float>("event_discard_odds")};

            // https://dv-processing.inivation.com/rel_1_7/reading_data.html#read-events-from-a-file
            if (data_reader_ptr->isRunning("events"))
            {
                if (const auto events = data_reader_ptr->getNextEventBatch(); events.has_value())
                {
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
                    }
                }
            }
            return data_read;
        }

        /**
         * For dynamic loading (streaming), gets a batch of frame data.
         * @param evt_data EventData object to populate with event/frame data
         * @param param_store ParameterStore object with data from GUI.
         * @return true if data was read, false otherwise.
         */
        bool get_frame_batch_data(EventData &evt_data, ParameterStore &param_store)
        {
            // If reader is not initialized, return immediately
            if (!data_reader_ptr)
            {
                return false;
            }
            bool data_read = false;
            // https://dv-processing.inivation.com/rel_1_7/reading_data.html#read-frames-from-a-file
            if (data_reader_ptr->isRunning("frames"))
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
                }
            }

            return data_read;
        }

    private:
        std::shared_ptr<dv::io::MonoCameraRecording> data_reader_ptr;
        int32_t camera_width;
        int32_t camera_height;

        // From old NOVA source code
        // to get random float from 0.0 to 1.0
        float randFloat()
        {
            return static_cast<float>(rand()) / RAND_MAX;
        };
};

#endif // DATA_ACQUISITION_HH
