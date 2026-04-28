#pragma once
#ifndef DATA_ACQUISITION_HH
#define DATA_ACQUISITION_HH

#include "data/DataSource.hh"
#include "data/DVEventReader.hh"
#include "data/EventData.hh"
#include "data/IEventReader.hh"
#include "data/MetavisionEventReader.hh"
#include "ui/Scrubber.hh"
#include "util/ErrorQueue.hh"
#include "render/GPUDevice.hh"

#include <mutex>
#include <opencv2/imgproc.hpp>
#include <shared_mutex>
#include <vector>

#include <dv-processing/io/camera/discovery.hpp>
#include <dv-processing/io/camera/usb_device.hpp>

/**
 * @brief This class provides a nice wrapper for managing and creating multiple DataSource's in a thread-safe way
 *
 */
class DataAcquisition
{
    private:
        SDL_GPUDevice *gpu_device;

        // Available devices
        std::vector<DataSource::ScannedCamera> scanned_cameras;
        std::vector<std::string> scanned_camera_names;

        // Currently available sources
        Scrubber::State shared_scrubber_state;
        std::vector<std::shared_ptr<DataSource>> data_sources;

    public:
        DataAcquisition(GPUDevice& gpu_device);

        std::shared_ptr<DataSource> add_camera_source(int camera_index);
        std::shared_ptr<DataSource> add_file_source(const std::string &file_path);
        void remove_data_source(size_t index);

        void discover_cameras();
        std::vector<std::string> get_scanned_camera_names();
        std::vector<std::shared_ptr<DataSource>> get_data_sources();

        int size();
        std::shared_ptr<DataSource> at(int index);

        void set_state(Scrubber::State state);
        Scrubber::State get_state();
        void sync_start();
        void sync_end();
        void update();
};

#endif // DATA_ACQUISITION_HH
