#pragma once
#ifndef DATA_WRITER_HH
#define DATA_WRITER_HH

#include "ErrorQueue.hh"

#include <dv-processing/io/mono_camera_recording.hpp>
#include <dv-processing/io/mono_camera_writer.hpp>

#include <queue>
#include <mutex>
#include <shared_mutex>


class DataAcquisition;

/**
 * @brief This class provides functionality to write event and frame data
 *        to an output aedat4 file. Works on a worker queue model.
 */
class DataWriter
{
    private:
        mutable std::shared_mutex mutex;
        
        // Modules 
        ErrorQueue &error_queue;

        // data writer pointer
        std::unique_ptr<dv::io::MonoCameraWriter> data_writer_ptr;

        // Queues of event & frame data
        std::queue<dv::EventStore> writer_event_queue;
        std::queue<dv::Frame> writer_frame_queue;

        // Toggles for GUI to save these values starting the next reader initialized
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
        DataWriter(ErrorQueue &error_queue): error_queue(error_queue) {}
    
        /**
         * @brief Sets up the DataWriter object for writing data to a file.
         * @param data_acq DataAcquisition object that will write data to DataWriter.
         * @param data_writer DataWriter object that will write data to file.
         * @param param_store ParameterStore object that contains global data from GUI.
         * @param prog_state State of the program.
         */
        void setup(DataAcquisition &data_acq)
        {   
            clear(); // clear() also uses lock so make sure this is before the new lock is created
            std::unique_lock dw_read_write_lock(mutex); 
            
            // Ensure the output file has a valid type
            if (!stream_save_file_name.ends_with(".aedat4")) stream_save_file_name.append(".aedat4");

            // If we are reading from a file, make sure the output file doesn't have the same name as the input file
            if (data_acq.get_state() == DataAcquisition::STATE::FILE_STREAM)
            {
                if (stream_save_file_name == data_acq.get_file_stream_name())
                {
                    size_t aedat_index = stream_save_file_name.find(".aedat4");
                    stream_save_file_name.insert(aedat_index, "new");
                }
            }

            
            // Check whether the user has toggled to save events and/or frames to the output file
            if (save_events_toggle || save_frames_toggle)
            {                   
                // Attempt to initialize the writer 
                if (init_data_writer(data_acq))
                {   

                    // Compose a message to tell user what is being saved
                    std::string saving_message = "Currently Saving ";

                    if (save_events_toggle)
                    {
                        saving_message.append("Event Data ");
                    }
                    if (save_frames_toggle)
                    {
                        saving_message.append(save_events_toggle ? "And Frame Data " : "Frame Data ");
                    }
                    saving_message.append("To ");
                    saving_message.append(stream_save_file_name);

                    // Reset saving controls
                    stream_save_file_name = "";
                    save_events_toggle = false;
                    save_frames_toggle = false;

                }
                else
                {   
                    // If initialization failed, just clear everything
                    error_queue.push_error("Data Reader Initialization failed for some god damn reason 🤷");
                    dw_read_write_lock.unlock();
                    clear(); // clear() requires the lock so you need to unlock prior
                }
            }
        }

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

            saving_message = "Nothing Being Saved Currently";
        }

        /**
         * @brief Initializes writer with DAVIS camera configs (event, frame, and IMU data).
         * 
         * NOT THREAD-SAFE, is only called within setup(), which acquires the mutex. I do this to
         * prevent any of the values changing between unlocking in setup() and relocking here in init_data_writer()
         * Blame phase 2
         * - Ryan
         * 
         * @return true if successful initialization of data writer, false otherwise.
         */
        bool init_data_writer(DataAcquisition &data_acq)
        {
            // Create config for writing all types of data (event, frame, IMU) for DAVIS Camera
            // https://dv-processing.inivation.com/131-add-wengen-to-dv-processing-2-0/writing_data.html
            try
            {
                dv::io::MonoCameraWriter::Config writer_config("Save Config");

                cv::Size file_res((std::max)(data_acq.get_camera_event_width(), data_acq.get_camera_frame_width()), 
                                  (std::max)(data_acq.get_camera_event_height(), data_acq.get_camera_frame_height()));

                writing_event_data = save_events_toggle;
                writing_frame_data = save_frames_toggle;

                if (save_events_toggle)
                {
                    writer_config.addEventStream(file_res);
                }

                if (save_frames_toggle)
                {
                    writer_config.addFrameStream(file_res);
                }

                data_writer_ptr = std::make_unique<dv::io::MonoCameraWriter>(stream_save_file_name, writer_config);
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
