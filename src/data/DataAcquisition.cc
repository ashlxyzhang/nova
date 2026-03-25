#include "data/DataAcquisition.hh"
#include <dv-processing/io/camera/discovery.hpp>
#include <dv-processing/io/camera/usb_device.hpp>

DataAcquisition::DataAcquisition(SDL_GPUDevice* gpu_device): gpu_device(gpu_device) {}

void DataAcquisition::discover_cameras()
{
    std::unique_lock da_read_write_lock(mutex);
    scanned_cameras.clear();
    scanned_camera_names.clear();
    const auto discovered_cameras{dv::io::camera::discover()};
    for (const auto &camera : discovered_cameras)
    {
        scanned_cameras.push_back(camera);
        std::stringstream str_stream;
        str_stream << "Model: " << camera.cameraModel << " ";
        str_stream << "Serial Number: " << camera.serialNumber << "\0";
        scanned_camera_names.push_back(str_stream.str());
    }
}

std::vector<std::string> DataAcquisition::get_scanned_camera_names()
{
    std::shared_lock da_read_lock(mutex);
    return scanned_camera_names;
}

void DataAcquisition::add_camera_source(int camera_index)
{
    std::unique_lock da_read_write_lock(mutex);
    if (camera_index >= 0 && camera_index < scanned_cameras.size())
    {
        std::shared_ptr<DataSource> new_source = std::make_shared<DataSource>(gpu_device, scanned_cameras[camera_index]);
        
        if (new_source->is_open()) 
        {
            data_sources.push_back(new_source);
        } 
        else 
        {
            std::cerr << "Failed to open camera source: " << scanned_camera_names[camera_index] << std::endl;
        }
    }
}

void DataAcquisition::add_file_source(const std::string& file_path)
{
    std::unique_lock da_read_write_lock(mutex);

    std::shared_ptr<DataSource> new_source = std::make_shared<DataSource>(gpu_device, file_path);  

    if (new_source->is_open()) 
    {
        data_sources.push_back(std::make_shared<DataSource>(gpu_device, file_path));   
    }
    else 
    {
        std::cerr << "Failed to open file source: " << file_path << std::endl;
    }
}

void DataAcquisition::remove_data_source(size_t index)
{
    std::unique_lock da_read_write_lock(mutex);
    if (index < data_sources.size())
    {
        data_sources.erase(data_sources.begin() + index);
    }
}

