
#include "data/DataAcquisition.hh"
#include "data/DataSource.hh"
#include "data/MetavisionEventReader.hh"
#include <algorithm>
#include <metavision/hal/device/device_discovery.h>

DataAcquisition::DataAcquisition(GPUDevice& gpu_device) : gpu_device(gpu_device.get_SDL_device())
{
}

void DataAcquisition::discover_cameras()
{
    std::unique_lock da_read_write_lock(mutex);
    scanned_cameras.clear();
    scanned_camera_names.clear();
    const auto discovered_cameras{dv::io::camera::discover()};
    for (const auto &camera : discovered_cameras)
    {
        scanned_cameras.push_back(ScannedCamera{ScannedCamera::Vendor::DV, camera, {}});
        std::stringstream str_stream;
        str_stream << "[DV] Model: " << camera.cameraModel << " ";
        str_stream << "Serial Number: " << camera.serialNumber << "\0";
        scanned_camera_names.push_back(str_stream.str());
    }

    try
    {
        const auto prophesee_cameras = Metavision::DeviceDiscovery::list_available_sources();
        for (const auto &desc : prophesee_cameras)
        {
            ScannedCamera entry{ScannedCamera::Vendor::PROPHESEE, {}, desc.serial_};
            scanned_cameras.push_back(std::move(entry));
            std::stringstream str_stream;
            str_stream << "[Prophesee] Plugin: " << desc.plugin_name_ << " ";
            str_stream << "Serial Number: " << desc.serial_;
            scanned_camera_names.push_back(str_stream.str());
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Prophesee discovery failed: " << e.what() << std::endl;
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
    if (camera_index >= 0 && camera_index < (int)scanned_cameras.size())
    {
        const auto &entry = scanned_cameras[camera_index];
        std::shared_ptr<DataSource> new_source;
        if (entry.vendor == ScannedCamera::Vendor::DV)
        {
            new_source = std::make_shared<DataSource>(gpu_device, entry.dv_descriptor);
        }
        else
        {
            new_source =
                std::make_shared<DataSource>(gpu_device, MetavisionEventReader::LiveCamera{entry.prophesee_serial});
        }

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

void DataAcquisition::add_file_source(const std::string &file_path)
{
    std::unique_lock da_read_write_lock(mutex);

    std::shared_ptr<DataSource> new_source = std::make_shared<DataSource>(gpu_device, file_path);

    if (new_source->is_open())
    {
        data_sources.push_back(new_source);
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

std::vector<std::shared_ptr<DataSource>> DataAcquisition::get_data_sources()
{
    std::shared_lock da_read_lock(mutex);
    return data_sources;
}

void DataAcquisition::set_state(Scrubber::State state)
{
    std::unique_lock da_read_write_lock(mutex);
    shared_scrubber_state = state;
}

Scrubber::State DataAcquisition::get_state()
{
    std::unique_lock da_read_write_lock(mutex);

    shared_scrubber_state.max_index = 0;
    shared_scrubber_state.max_time = 0;

    // Lazily update the upper bounds of the shared state before returning
    for (std::shared_ptr<DataSource> data_source : data_sources)
    {
        Scrubber::State state = data_source->scrubber.state;
        shared_scrubber_state.max_index = (std::max)(shared_scrubber_state.max_index, state.max_index);
        shared_scrubber_state.max_time = (std::max)(shared_scrubber_state.max_time, state.max_time);
    }

    return shared_scrubber_state;
}

void DataAcquisition::sync_start()
{
    // Make copy of shared/synced scubber state
    Scrubber::State synced = get_state();

    // Apply controls to each individual data_source
    for (std::shared_ptr<DataSource> data_source : data_sources)
    {
        Scrubber::State state = data_source->scrubber.state;

        // Copy scrubbing parameters
        state.type = synced.type;
        state.mode = synced.mode;
        state.time_window = synced.time_window;
        state.time_step = synced.time_step;
        state.index_window = synced.index_window;
        state.index_step = synced.index_step;

        // To sync to start, simply use the current_index and current_time as is
        state.current_time = (std::min)(synced.current_time, state.max_time);
        state.current_index = (std::min)(synced.current_index, state.max_index);

        // Reapply state
        data_source->scrubber.state = state;
    }
}

void DataAcquisition::sync_end()
{
    // Make copy of shared/synced scubber state
    Scrubber::State synced = get_state();
    int synced_index_length = (int)synced.max_index - (int)synced.min_index + 1;
    float synced_time_length = synced.max_time - synced.min_time;

    // Apply controls to each individual data_source
    for (std::shared_ptr<DataSource> data_source : data_sources)
    {
        Scrubber::State state = data_source->scrubber.state;

        // Copy scrubbing parameters
        state.type = synced.type;
        state.mode = synced.mode;
        state.time_window = synced.time_window;
        state.time_step = synced.time_step;
        state.index_window = synced.index_window;
        state.index_step = synced.index_step;

        // Sync time to end
        float current_time = synced.current_time;
        float time_length = state.max_time - state.min_time;
        state.current_time = std::clamp(current_time - (synced_time_length - time_length), 0.0f, state.max_time);

        // Sync event to end
        int current_index = (int)synced.current_index;
        int index_length = (int)state.max_index - (int)state.min_index + 1;
        state.current_index = std::clamp(current_index - (synced_index_length - index_length), 0, (int)state.max_index);

        // Reapply state
        data_source->scrubber.state = state;
    }
}

int DataAcquisition::size()
{
    std::shared_lock da_read_lock(mutex);
    return (int)data_sources.size();
}

std::shared_ptr<DataSource> DataAcquisition::at(int index)
{
    std::shared_lock da_read_lock(mutex);
    return data_sources.at(index);
}

void DataAcquisition::update()
{
    std::unique_lock da_read_write_lock(mutex);

    // Shared state doesn't need full update since there is no associated event_data & data to upload
    shared_scrubber_state.step_forward();

    // Each data source does a full update
    for (const auto &data_source : data_sources)
    {
        data_source->update();
    }
}
