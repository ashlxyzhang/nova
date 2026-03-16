#pragma once
#ifndef DATA_ACQUISITION_HH
#define DATA_ACQUISITION_HH

#include "EventData.hh"
#include "ErrorQueue.hh"
#include "IEventReader.hh"
#include "DVEventReader.hh"
#include "MetavisionEventReader.hh"

#include <dv-processing/io/camera/discovery.hpp>
#include <dv-processing/io/camera/usb_device.hpp>
#include <opencv2/imgproc.hpp>

#include <vector>
#include <mutex>
#include <shared_mutex>

class DataWriter;

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
    public:
        enum class STATE : uint8_t
        {
            IDLE = 0,        
            FILE_STREAM = 2,  
            CAMERA_STREAM = 3
        };

    private:
        mutable std::shared_mutex mutex;
        // Modules
        ErrorQueue &error_queue;
        // -----

        // Available camera
        std::vector<dv::io::camera::USBDevice::DeviceDescriptor> scanned_cameras;
        std::vector<std::string> scanned_camera_names;
        // -----

        // Reader state
        STATE state = STATE::IDLE;
        std::unique_ptr<IEventReader> data_reader_ptr;

        int32_t camera_event_width;
        int32_t camera_event_height;
        int32_t camera_frame_width;
        int32_t camera_frame_height;

        float event_discard_odds = 1.0f; // 1.0 keeps all, 0.0 keeps none
        // -----

        // Selected camera        
        int32_t camera_index = -1; // -1 if none selected

        bool camera_stream_paused = false;
        bool camera_stream_changed = false; 
        // -----
        // Selected file
        std::string file_stream_name = "";
        bool file_stream_paused = false; 
        bool file_stream_changed = false; 
        // -----

        float randFloat()
        {
            return static_cast<float>(rand()) / RAND_MAX;
        };


    public:
        /**
         * @param error_queue ErrorQueue object used to report errors to be displayed and/or logged
         */
        DataAcquisition(ErrorQueue &error_queue);


        /**
         * @brief Clears every member variable
         */
        void clear();


        /**
         * @brief Scan for available USB cameras load camera names and their pointers into scanned_camera_names and
         * scanned_cameras respectively
         */
        void discover_cameras();

        /**
         * @brief Loads camera to read. Initializes internal reader with camera.
         * @return false if failed to init reader, true otherwise.
         */
        bool init_camera_reader();

        /**
         * @brief Loads file to read. Initializes internal reader based on file extension:
         *          .aedat4        -> dv-processing backend
         *          .raw / .dat    -> OpenEB/Metavision backend
         * @return false if failed to init reader, true otherwise.
         */
        bool init_file_reader();

        /**
         * @brief Gives event camera resolution to event data.
         * @param evt_data EventData object to give camera resolution to.
         */
        void get_camera_event_resolution(EventData &evt_data);

        /**
         * @brief Gives frame camera resolution to event data.
         * @param evt_data EventData object to give camera resolution to.
         */
        void get_camera_frame_resolution(EventData &evt_data);

        /**
         * @brief For dynamic loading (streaming), gets a batch of event data.
         * @param evt_data EventData object to populate with event data.
         * @param data_writer DataWriter for optional persistent storage.
         * @return true if any data was read, false otherwise.
         */
        bool get_batch_evt_data(EventData &evt_data, DataWriter &data_writer);

        /**
         * @brief For dynamic loading (streaming), gets a batch of frame data.
         * @param evt_data EventData object to populate with frame data.
         * @param data_writer DataWriter for optional persistent storage.
         * @return true if data was read, false otherwise.
         */
        bool get_batch_frame_data(EventData &evt_data, DataWriter &data_writer);

        /**
         * @brief Returns event camera width.
         * @return event_camera_width
         */
        int32_t get_camera_event_width();

        /**
         * @brief Returns frame camera width.
         * @return frame_camera_width
         */
        int32_t get_camera_frame_width();

        /**
         * @brief Returns event camera height.
         * @return event_camera_height
         */
        int32_t get_camera_event_height();

        /**
         * @brief Returns frame camera height.
         * @return frame_camera_height
         */
        int32_t get_camera_frame_height();


        // Thread-safe getters
        STATE get_state();
        float get_event_discard_odds();
        int32_t get_camera_index();
        bool is_camera_stream_paused();
        bool has_camera_stream_changed();
        std::string get_file_stream_name();
        bool is_file_stream_paused();
        bool has_file_stream_changed();
        std::vector<std::string> get_scanned_camera_names();


        // Thread-safe setters
        void set_event_discard_odds(float odds);
        void set_camera_index(int32_t index);
        void set_camera_stream_paused(bool paused);
        void set_camera_stream_changed(bool changed);
        void set_file_stream_name(const std::string &name);
        void set_file_stream_paused(bool paused);
        void set_file_stream_changed(bool changed);
        void set_state(const STATE new_state);
};



#endif // DATA_ACQUISITION_HH