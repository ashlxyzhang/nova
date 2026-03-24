#include "data/DataAcquisition.hh"
#include "data/DataWriter.hh"

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

void DataAcquisition::add_camera_source(const dv::io::camera::USBDevice::DeviceDescriptor& camera)
{
    std::unique_lock da_read_write_lock(mutex);
    data_sources.push_back(std::make_shared<DataSource>(camera));
}

void DataAcquisition::add_file_source(const std::string& file_path)
{
    std::unique_lock da_read_write_lock(mutex);
    data_sources.push_back(std::make_shared<DataSource>(file_path));   
}

