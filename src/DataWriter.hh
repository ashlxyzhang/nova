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
        std::unique_ptr<dv::io::MonoCameraWriter> data_writer_ptr;

        std::mutex writer_lock; // For thread safety

        // Queue of event stores
        std::queue<dv::EventStore> writer_event_queue;

        // Queue of frame data
        std::queue<dv::Frame> writer_frame_queue;

        // Determine if writing event or frame data
        bool writing_frame_data;
        bool writing_event_data;

    public:
        DataWriter()
            : data_writer_ptr{}, writer_lock{}, writer_event_queue{}, writer_frame_queue{}, writing_frame_data{false},
              writing_event_data{false}
        {
        }

        /**
         * @brief Getter for writing_frame_data
         * @return value of writing_frame_data
         */
        bool get_writing_frame_data()
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};
            bool ret_bool{writing_frame_data};
            writer_lock_ul.unlock();
            return ret_bool;
        }

        /**
         * @brief Getter for writing_event_data
         * @return value of writing_event_data
         */
        bool get_writing_event_data()
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};
            bool ret_bool{writing_event_data};
            writer_lock_ul.unlock();
            return ret_bool;
        }

        /**
         * @brief Clears all data from internal structures.
         */
        void clear()
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};
            data_writer_ptr.reset();

            // Clear states
            writing_frame_data = false;
            writing_event_data = false;

            // Clear queues
            while (!writer_event_queue.empty())
            {
                writer_event_queue.pop();
            }

            while (!writer_frame_queue.empty())
            {
                writer_frame_queue.pop();
            }
            writer_lock_ul.unlock();
        }

        /**
         * @brief Initializes writer with DAVIS camera configs (event, frame, and IMU data).
         * @param file_name Output file of data.
         * @param _camera_event_width Width of camera event resolution.
         * @param _camera_event_height Height of camera event resolution.
         * @param _camera_frame_width Width of camera frame resolution.
         * @param _camera_frame_height Height of camera frame resolution.
         * @param event_data True if event stream is being written.
         * @param frame_data True if frame stream is being written.
         * @param param_store ParameterStore object to store error message into
         * @return true if successful initialization of data writer, false otherwise.
         */
        bool init_data_writer(const std::string &file_name, int32_t _camera_event_width, int32_t _camera_event_height,
                              int32_t _camera_frame_width, int32_t _camera_frame_height, bool event_data,
                              bool frame_data, ParameterStore &param_store)
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};

            // Create config for writing all types of data (event, frame, IMU) for DAVIS Camera
            // https://dv-processing.inivation.com/131-add-wengen-to-dv-processing-2-0/writing_data.html
            try
            {
                dv::io::MonoCameraWriter::Config writer_config("Save Config");

                cv::Size file_res{std::max(_camera_event_width, _camera_frame_width),
                                  std::max(_camera_event_height, _camera_frame_height)};

                writing_event_data = event_data;
                writing_frame_data = frame_data;
                if (event_data)
                {
                    writer_config.addEventStream(file_res);
                }

                if (frame_data)
                {
                    writer_config.addFrameStream(file_res);
                }

                std::string file_name_appended{file_name};
                if (!file_name_appended.ends_with(".aedat4"))
                {
                    file_name_appended.append(".aedat4");
                }
                data_writer_ptr = std::make_unique<dv::io::MonoCameraWriter>(file_name_appended, writer_config);
            }
            catch (...)
            {
                std::string pop_up_err_str{"Something went wrong initializing file to save to!"};
                param_store.add("pop_up_err_str", pop_up_err_str);
                writer_lock_ul.unlock();
                return false;
            }

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
         * @param param_store ParameterStore object to store popup error message into
         *                    in case writing fails.
         * @return false if write failed, true otherwise.
         */
        bool write_event_store(ParameterStore &param_store)
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};

            if (writer_event_queue.empty() || !data_writer_ptr || !writing_event_data)
            {
                writer_lock_ul.unlock();
                return false;
            }

            dv::EventStore evt_store{writer_event_queue.front()};
            writer_event_queue.pop();

            try
            {
                data_writer_ptr->writeEvents(evt_store);
            }
            catch (...)
            {
                std::string pop_up_err_str{"Something went wrong with saving event data!"};
                param_store.add("pop_up_err_str", pop_up_err_str);
                writer_lock_ul.unlock();
                return false;
            }

            writer_lock_ul.unlock();
            return true;
        }

        /**
         * @brief Pops an frame data from queue and writes
         *        it to persistent storage.
         * @param param_store ParameterStore object to store popup error message into
         *                    in case writing fails.
         * @return false if write failed, true otherwise.
         */
        bool write_frame_data(ParameterStore &param_store)
        {
            std::unique_lock<std::mutex> writer_lock_ul{writer_lock};

            if (writer_frame_queue.empty() || !data_writer_ptr || !writing_frame_data)
            {
                writer_lock_ul.unlock();
                return false;
            }

            dv::Frame frame_data{writer_frame_queue.front()};
            writer_frame_queue.pop();

            try
            {
                data_writer_ptr->writeFrame(frame_data);
            }
            catch (...)
            {
                std::string pop_up_err_str{"Something went wrong with saving frame data!"};
                param_store.add("pop_up_err_str", pop_up_err_str);
                writer_lock_ul.unlock();
                return false;
            }

            writer_lock_ul.unlock();
            return true;
        }
};

#endif // DATA_WRITER_HH
