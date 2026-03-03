#pragma once
#ifndef DATA_WRITER_HH
#define DATA_WRITER_HH

#include "ParameterStore.hh"
#include "ErrorQueue.hh"

#include <dv-processing/io/mono_camera_recording.hpp>
#include <dv-processing/io/mono_camera_writer.hpp>

#include <queue>

/**
 * @brief This class provides functionality to write event and frame data
 *        to an output aedat4 file. Works on a worker queue model.
 */
class DataWriter
{
    private:
        std::shared_mutex mutex;
        
        // Modules 
        ErrorQueue &error_queue;

        // data writer pointer
        std::unique_ptr<dv::io::MonoCameraWriter> data_writer_ptr;

        // Queues of event & frame data
        std::queue<dv::EventStore> writer_event_queue;
        std::queue<dv::Frame> writer_frame_queue;

        // Toggles for GUI
        bool save_events_toggle = false;
        bool save_frames_toggle = false;

        // Currently writing data flags
        bool writing_frame_data = false;
        bool writing_event_data = false;

        // Name of file to save data to and GUI message 
        std::string stream_save_file_name = "";
        std::string saving_message = "Nothing Being Saved Currently";

    public:

        void set_save_frames_toggle(const bool toggle) {
            std::unique_lock lock(mutex);
            save_frames_toggle = toggle;
        }

        bool get_save_frames_toggle() {
            std::shared_lock lock(mutex);
            return save_frames_toggle;
        }

        void set_save_events_toggle(const bool toggle) {
            std::unique_lock lock(mutex);
            save_events_toggle = toggle;
        }

        bool get_save_events_toggle() {
            std::shared_lock lock(mutex);
            return save_events_toggle;
        }

        void set_saving_message(const std::string& message) {
            std::unique_lock lock(mutex);
            saving_message = message;
        }

        std::string get_saving_message() {
            std::shared_lock lock(mutex);
            return saving_message;
        }

        void set_stream_save_file_name(const std::string &file_name) {
            std::unique_lock lock(mutex);
            stream_save_file_name = file_name;
        }

        std::string get_stream_save_file_name() {
            std::shared_lock lock(mutex);
            return stream_save_file_name;
        }

        /**
         * @brief Getter for writing_frame_data. DataAcquisition should use to determine if writing frame data or not.
         * @return value of writing_frame_data (is the object writing frame data or not)
         */
        bool get_writing_frame_data()
        {
            std::shared_lock lock(mutex);
            return writing_frame_data;
        }

        /**
         * @brief Getter for writing_event_data. DataAcquisition should use to determine if writing event data or not.
         * @return value of writing_event_data (is the object writing event data or not)
         */
        bool get_writing_event_data()
        {
            std::shared_lock lock(mutex);
            return writing_event_data;
        }


        /**
         * @brief Constructor, zero initializes all values
         */
        DataWriter(ErrorQueue &error_queue):    data_writer_ptr{}, writer_event_queue{}, writer_frame_queue{}, 
                                                writing_frame_data{false}, writing_event_data{false}, 
                                                error_queue(error_queue) {}


        /**
         * @brief Clears all data from internal structures.
         */
        void clear()
        {
            std::unique_lock dw_read_write_lock(mutex);
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
                              bool frame_data)
        {
            std::unique_lock dw_read_write_lock(mutex);

            // Create config for writing all types of data (event, frame, IMU) for DAVIS Camera
            // https://dv-processing.inivation.com/131-add-wengen-to-dv-processing-2-0/writing_data.html
            try
            {
                dv::io::MonoCameraWriter::Config writer_config("Save Config");

                cv::Size file_res((std::max)(_camera_event_width, _camera_frame_width), (std::max)(_camera_event_height, _camera_frame_height));

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
                error_queue.push_error("Something went wrong initializing file to save to!");
                return false;
            }

            return true;
        }

        /**
         * @brief Adds event data store to queue.
         * @param evt_store dv::EventStore object containing event data to write.
         */
        void add_event_store(dv::EventStore evt_store)
        {
            std::unique_lock dw_read_write_lock(mutex);
            writer_event_queue.push(evt_store);
        }

        /**
         * @brief Adds frame data to queue.
         * @param frame_data dv::Frame object containing frame data to write.
         */
        void add_frame_data(dv::Frame frame_data)
        {
            std::unique_lock dw_read_write_lock(mutex);
            writer_frame_queue.push(frame_data);
        }

        /**
         * @brief Pops an event data store from queue and writes
         *        it to persistent storage.
         * @param param_store ParameterStore object to store popup error message into
         *                    in case writing fails.
         * @return false if write failed, true otherwise.
         */
        bool write_event_store()
        {
            std::unique_lock dw_read_write_lock(mutex);

            if (writer_event_queue.empty() || !data_writer_ptr || !writing_event_data) return false;
        
            dv::EventStore evt_store{writer_event_queue.front()};
            writer_event_queue.pop();

            try
            {
                data_writer_ptr->writeEvents(evt_store);
            }
            catch (...)
            {
                error_queue.push_error("Something went wrong with saving event data!");
                return false;
            }

            return true;
        }

        /**
         * @brief Pops an frame data from queue and writes
         *        it to persistent storage.
         * @param param_store ParameterStore object to store popup error message into
         *                    in case writing fails.
         * @return false if write failed, true otherwise.
         */
        bool write_frame_data()
        {
            std::unique_lock dw_read_write_lock(mutex);

            if (writer_frame_queue.empty() || !data_writer_ptr || !writing_frame_data) return false;

            dv::Frame frame_data{writer_frame_queue.front()};
            writer_frame_queue.pop();

            try
            {
                data_writer_ptr->writeFrame(frame_data);
            }
            catch (...)
            {
                error_queue.push_error("Something went wrong with saving frame data!");
                return false;
            }

            return true;
        }
};

#endif // DATA_WRITER_HH
