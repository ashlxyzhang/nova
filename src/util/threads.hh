#pragma once
#ifndef THREADS_HH
#define THREADS_HH

#include "data/DataAcquisition.hh"
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
 * @brief Thread for data acquisition, storing into event_data
 * @param running Atomic boolean that determines if thread is running or not.
 * @param data_acq DataAcquisition object that acquires data from cameras and files.
 */
// Thread for data acquisition, storing into event_data
inline void data_acquisition_thread(std::atomic<bool> &running, DataAcquisition &data_acq)
{
    while (running)
    {

        // Get copy of all DataSource's
        std::vector<std::shared_ptr<DataSource>> data_sources = data_acq.get_data_sources();
        for (const auto& data_source: data_sources)
        {
            
            // Lock data source for reading
            std::shared_lock data_source_lock(data_source->mutex);

            if (data_source->state == DataSource::State::ACTIVE)
            {
                data_source->get_batch_event_data();
                data_source->get_batch_frame_data();
            }
        }
    }
}
} // namespace program_thread

#endif // THREADS_HH
