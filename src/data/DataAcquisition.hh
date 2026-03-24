#pragma once
#ifndef DATA_ACQUISITION_HH
#define DATA_ACQUISITION_HH

#include "data/DVEventReader.hh"
#include "data/EventData.hh"
#include "data/IEventReader.hh"
#include "data/MetavisionEventReader.hh"
#include "data/DataSource.hh"
#include "util/ErrorQueue.hh"

#include <dv-processing/io/camera/discovery.hpp>
#include <dv-processing/io/camera/usb_device.hpp>
#include <opencv2/imgproc.hpp>

#include <mutex>
#include <shared_mutex>
#include <vector>

class DataWriter;

/**
 * @brief This class provides a nice wrapper for managing and creating multiple DataSource's in a thread-safe way
 * 
 */
class DataAcquisition
{
    private:
        mutable std::shared_mutex mutex;
        
        SDL_GPUDevice* gpu_device;
        
        std::vector<dv::io::camera::USBDevice::DeviceDescriptor> scanned_cameras;
        std::vector<std::string> scanned_camera_names;
        std::vector<std::shared_ptr<DataSource>> data_sources; 

    public:

        DataAcquisition(SDL_GPUDevice* gpu_device);

        void discover_cameras();
        void add_camera_source(int camera_index);
        void add_file_source(const std::string& file_path);
        void remove_data_source(size_t index);

        std::vector<std::string> get_scanned_camera_names();        
        std::vector<dv::io::camera::USBDevice::DeviceDescriptor> get_scanned_cameras();
        std::vector<std::shared_ptr<DataSource>> get_data_sources(); 
};

#endif // DATA_ACQUISITION_HH
