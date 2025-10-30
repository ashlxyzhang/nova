#pragma once
#ifndef DATA_WRITER_HH
#define DATA_WRITER_HH

#include "ParameterStore.hh"
#include <dv-processing/io/mono_camera_recording.hpp>
#include <dv-processing/io/mono_camera_writer.hpp>
#include <queue>

class DataWriter
{

    private:
        std::shared_ptr<dv::io::MonoCameraWriter> data_writer_ptr;
        int32_t camera_width;
        int32_t camera_height;

        std::mutex writer_lock; // For thread safety

        // Queue of event stores
        std::queue<dv::EventStore> writer_event_queue;

        // Queue of frame data
        std::queue<dv::Frame> writer_frame_queue;

    public:
        DataWriter()
            : data_writer_ptr{}, camera_width{}, camera_height{}, writer_lock{}, writer_event_queue{},
              writer_frame_queue{}
        {
        }

        /**
         * @brief Clears all data from internal structures.
         */
        void clear()
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};
            data_writer_ptr.reset();
            camera_width = 0;
            camera_height = 0;
            // Clear queues
            while(!writer_event_queue.empty())
            {
                writer_event_queue.pop();
            }

            while(!writer_frame_queue.empty())
            {
                writer_frame_queue.pop();
            }

        }

        /**
         * @brief Initializes writer with DAVIS camera configs (event, frame, and IMU data).
         * @param file_name Output file of data.
         * @param camera_width Width of camera resolution.
         * @param camera_height Height of camera resolution.
         */
        bool init_data_writer(const std::string &file_name, int32_t camera_width, int32_t camera_height)
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};

            // Create config for writing all types of data (event, frame, IMU) for DAVIS Camera
            // https://dv-processing.inivation.com/131-add-wengen-to-dv-processing-2-0/writing_data.html
            const auto writer_config{
                dv::io::MonoCameraWriter::DAVISConfig("DAVISConfig", cv::Size(camera_height, camera_width))};
            std::string file_name_appended{file_name};
            file_name_appended.append(".aedat4");
            data_writer_ptr = std::make_shared<dv::io::MonoCameraWriter>(file_name_appended, writer_config);

            writer_lock_ul.unlock();

            return true;
        }

        /**
         * @brief Adds event data store to queue.
         */
        void add_event_store(dv::EventStore evt_store)
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};
            writer_event_queue.push(evt_store);
            writer_lock_ul.unlock();
        }

        /**
         * @brief Adds frame data to queue
         */
        void add_frame_data(dv::Frame frame_data)
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};
            writer_frame_queue.push(frame_data);
            writer_lock_ul.unlock();
        }

        /**
         * @brief Pops an event data store from queue and writes
         *        it to persistent storage.
         * @return false if write failed, true otherwise.
         */
        bool write_event_store()
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};

            if (writer_event_queue.empty() || !data_writer_ptr)
            {
                writer_lock_ul.unlock();
                return false;
            }

            dv::EventStore evt_store{writer_event_queue.front()};
            writer_event_queue.pop();

            data_writer_ptr->writeEvents(evt_store);

            writer_lock_ul.unlock();
            return true;
        }

        /**
         * @brief Pops an frame data from queue and writes
         *        it to persistent storage.
         * @return false if write failed, true otherwise.
         */
        bool write_frame_data()
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};

            if (writer_frame_queue.empty() || !data_writer_ptr)
            {
                writer_lock_ul.unlock();
                return false;
            }

            dv::Frame frame_data{writer_frame_queue.front()};
            writer_frame_queue.pop();

            data_writer_ptr->writeFrame(frame_data);

            writer_lock_ul.unlock();
            return true;
        }
};

#endif // DATA_WRITER_HH
