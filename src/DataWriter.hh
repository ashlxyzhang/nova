#pragma once
#ifndef DATA_WRITER_HH
#define DATA_WRITER_HH

#include "ErrorQueue.hh"
#include "EventData.hh"

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
        bool init_data_writer(DataAcquisition &data_acq);

    public:

        void set_save_frames_toggle(const bool toggle);
        bool get_save_frames_toggle();
        void set_save_events_toggle(const bool toggle);
        bool get_save_events_toggle();
        void set_saving_message(const std::string& message);
        std::string get_saving_message();
        void set_stream_save_file_name(const std::string &file_name);
        std::string get_stream_save_file_name();

        /**
         * @brief Getter for writing_frame_data. DataAcquisition should use to determine if writing frame data or not.
         * @return value of writing_frame_data (is the object writing frame data or not)
         */
        bool get_writing_frame_data();

        /**
         * @brief Getter for writing_event_data. DataAcquisition should use to determine if writing event data or not.
         * @return value of writing_event_data (is the object writing event data or not)
         */
        bool get_writing_event_data();


        /**
         * @brief Constructor, zero initializes all values
         */
        DataWriter(ErrorQueue &error_queue);

        /**
         * @brief Sets up the DataWriter object for writing data to a file.
         * @param data_acq DataAcquisition object that will write data to DataWriter.
         * @param data_writer DataWriter object that will write data to file.
         * @param param_store ParameterStore object that contains global data from GUI.
         * @param prog_state State of the program.
         */
        void setup(DataAcquisition &data_acq);

        /**
         * @brief Clears all data from internal structures.
         */
        void clear();

        /**
         * @brief Adds event data store to queue.
         * @param evt_store dv::EventStore object containing event data to write.
         */
        void add_event_store(dv::EventStore evt_store);

        /**
         * @brief Adds frame data to queue.
         * @param frame_data dv::Frame object containing frame data to write.
         */
        void add_frame_data(dv::Frame frame_data);

        /**
         * @brief Pops an event data store from queue and writes
         *        it to persistent storage.
         * @param param_store ParameterStore object to store popup error message into
         *                    in case writing fails.
         * @return false if write failed, true otherwise.
         */
        bool write_event_store();

        /**
         * @brief Pops an frame data from queue and writes
         *        it to persistent storage.
         * @param param_store ParameterStore object to store popup error message into
         *                    in case writing fails.
         * @return false if write failed, true otherwise.
         */
        bool write_frame_data();
};

#endif // DATA_WRITER_HH