#pragma once
#ifndef DATA_ACQUISITION_HH
#define DATA_ACQUISITION_HH

#include "EventData.hh"
#include <dv-processing/io/mono_camera_recording.hpp>

class DataAcquisition
{
    public:
        DataAcquisition(const std::string &filename)
            : data_reader{filename}, camera_width{data_reader.getEventResolution().value().width},
              camera_height{data_reader.getEventResolution().value().width}
        {
        }

        bool get_evt_batch_data(EventData &evt_data)
        {
            bool data_read = false;
            // https://dv-processing.inivation.com/rel_1_7/reading_data.html#read-events-from-a-file
            if (data_reader.isRunning("events"))
            {
                if (const auto events = data_reader.getNextEventBatch(); events.has_value())
                {
                    for (auto &evt : events.value())
                    {

                        EventData::EventDatum evt_datum{
                            .x = evt.x(), .y = evt.y(), .timestamp = evt.timestamp(), .polarity = evt.polarity()};

                        evt_data.write_evt_data(evt_datum);
                        data_read = true;
                    }
                }
            }
            return data_read;
        }

        bool get_frame_batch_data(EventData &evt_data)
        {

            bool data_read = false;
            // https://dv-processing.inivation.com/rel_1_7/reading_data.html#read-frames-from-a-file
            if (data_reader.isRunning("frames"))
            {
                if (const auto frame_data = data_reader.getNextFrame(); frame_data.has_value())
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
        dv::io::MonoCameraRecording data_reader;
        int32_t camera_width;
        int32_t camera_height;
};

#endif // DATA_ACQUISITION_HH
