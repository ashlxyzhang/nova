#pragma once
#ifndef THREADS_HH
#define THREADS_HH

#include "data/DataAcquisition.hh"
#include "data/DataWriter.hh"
#include "data/EventData.hh"
#include "render/DigitalCodedExposure.hh"
#include "ui/GUI.hh"
#include "ui/Scrubber.hh"

#include <mutex>
#include <shared_mutex>

// Program threads
/**
 * @brief Namespace for functions that serve as entrypoints to threads in this program.
 *        Consist of thread for data acquisition and thread for writing data.
 */
namespace program_thread
{
/**
 * @brief Thread for writing data back to persistent storage when streaming.
 * @param running Atomic boolean that determines if thread is running or not.
 * @param data_writer DataWriter object to write data with.
 * @param param_store ParameterStore object containing global data.
 *
 */

inline void writer_thread(std::atomic<bool> &running, DataWriter &data_writer)
{
    // For now let us spin
    while (running)
    {
        data_writer.write_event_store();
        data_writer.write_frame_data();
    }
}

/**
 * @brief Thread for data acquisition, storing into event_data
 * @param running Atomic boolean that determines if thread is running or not.
 * @param param_store ParameterStore object to get GUI info from.
 * @param evt_data EventData object to store event/frame data into for drawing.
 * @param data_writer DataWriter object to store event/frame data into to be saved to persistent storage.
 */
// Thread for data acquisition, storing into event_data
inline void data_acquisition_thread(std::atomic<bool> &running, DataAcquisition &data_acq, EventData &evt_data,
                                    DataWriter &data_writer, DigitalCodedExposure &dce, Scrubber &scrubber)
{
    while (running)
    {
        switch (data_acq.get_state())
        {

        // FILE STREAMING
        // -----------------------------------------------------------------------------------
        case DataAcquisition::STATE::FILE_STREAM:

            // If stream file changed, reset reader to read from new file and clear previously read event data
            if (data_acq.has_file_stream_changed())
            {

                // Clear DataAcqusition, EventData, and DataWriter
                data_acq.clear();
                evt_data.clear();
                data_writer.clear();
                scrubber.clear();

                // Attempt to initialize a afile reader for the selected file
                if (data_acq.init_file_reader())
                {
                    data_acq.get_camera_event_resolution(evt_data);
                    data_acq.get_camera_frame_resolution(evt_data);
                    data_acq.set_file_stream_changed(false);
                    dce.initialize_textures_next_update(); // DCE needs to reinitialize it's GPU textures for this new
                                                           // input
                }

                // If the user has set a file to save data to, setup a writer for that file
                if (data_writer.get_stream_save_file_name() != "")
                    data_writer.setup(data_acq);
            }

            // If not paused, repeatedly order (sternly) DataAcqusition to query cameras/files for events and/or frames
            if (!data_acq.is_file_stream_paused())
            {
                data_acq.get_batch_evt_data(evt_data, data_writer);
                data_acq.get_batch_frame_data(evt_data, data_writer);
            }

            break;

        // CAMERA STREAMING
        // -----------------------------------------------------------------------------------
        case DataAcquisition::STATE::CAMERA_STREAM:

            // If the selected camera has changed we need to reinitialize a bunch of stuff
            if (data_acq.has_camera_stream_changed())
            {

                // Clear DataAcqusition, EventData, and DataWriter
                data_acq.clear();
                evt_data.clear();
                data_writer.clear();
                scrubber.clear();

                // Attempt to initialize a camera reader for the selected camera
                if (data_acq.init_camera_reader())
                {
                    data_acq.get_camera_event_resolution(evt_data);
                    data_acq.get_camera_frame_resolution(evt_data);

                    data_acq.set_camera_stream_changed(false);
                    dce.initialize_textures_next_update(); // DCE needs to reinitialize it's GPU textures for this new
                                                           // input
                }

                // If the user has set a file to save data to, setup a writer for that file
                if (data_writer.get_stream_save_file_name() != "")
                    data_writer.setup(data_acq);
            }

            // Check if stream is paused
            if (!data_acq.is_camera_stream_paused())
            {
                // Get event/frame data in batches every frame
                data_acq.get_batch_evt_data(evt_data, data_writer);
                data_acq.get_batch_frame_data(evt_data, data_writer);
            }

            break;

        // NOTHING STREAMING
        // -----------------------------------------------------------------------------------
        case DataAcquisition::STATE::IDLE:
            // Nothing being saved
            // data_acq.clear();
            break;
        }
    }
}
} // namespace program_thread

#endif // THREADS_HH
