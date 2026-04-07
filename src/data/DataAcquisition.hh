#pragma once
#ifndef DATA_ACQUISITION_HH
#define DATA_ACQUISITION_HH

#include "data/DataSource.hh"
#include "ui/Scrubber.hh"
#include "data/DVEventReader.hh"
#include "data/EventData.hh"
#include "data/IEventReader.hh"
#include "data/MetavisionEventReader.hh"
#include "util/ErrorQueue.hh"

#include <opencv2/imgproc.hpp>
#include <mutex>
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
        mutable std::shared_mutex mutex;
        SDL_GPUDevice* gpu_device;

        // Available devices
        std::vector<dv::io::camera::USBDevice::DeviceDescriptor> scanned_cameras;
        std::vector<std::string> scanned_camera_names;

        // Currently available sources
        Scrubber::ScrubberState shared_scrubber_state;
        std::vector<std::shared_ptr<DataSource>> data_sources; 

    public:

        DataAcquisition(SDL_GPUDevice* gpu_device);

        void add_camera_source(int camera_index);
        void add_file_source(const std::string& file_path);
        void remove_data_source(size_t index);
        
        void discover_cameras();
        std::vector<std::string> get_scanned_camera_names();        
        std::vector<std::shared_ptr<DataSource>> get_data_sources(); 

        void set_state(Scrubber::ScrubberState state); 
        Scrubber::ScrubberState get_state(); 
        void sync_start();
        void sync_end();
        void update();
};

#endif // DATA_ACQUISITION_HH
