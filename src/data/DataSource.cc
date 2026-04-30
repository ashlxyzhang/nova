
#include "data/DataSource.hh"
#include "data/IEventReader.hh"
#include "data/DVEventReader.hh"
#include "data/MetavisionEventReader.hh"

#include <metavision/hal/device/device_discovery.h>
#include <memory>
#include <optional>
#include <filesystem>
#include <random>

DataSource::DataSource(SDL_GPUDevice* gpu_device, const std::string& file_path)
	: gpu_device(gpu_device), 
    transfer_buffer(gpu_device), 
    type(Type::FILE), 
    is_open_(true),
    scrubber(gpu_device)
{
    std::filesystem::path path(file_path);
    name = path.filename().string(); 

    // Load file based on extension
    std::string ext = path.extension().string();
    if (ext == ".aedat4")
    {
        try
        {
            reader = std::make_unique<DVEventReader>(std::make_unique<dv::io::MonoCameraRecording>(file_path));
        }
        catch (const std::exception &e)
        {
            std::cerr << "aedat4 reader error: " + std::string(e.what()) << std::endl;
            is_open_ = false;
            return;
        }
    }
    else if (ext == ".raw" || ext == ".dat")
    {
        try
        {
            reader = std::make_unique<MetavisionEventReader>(file_path);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Metavision reader error: " + std::string(e.what()) << std::endl;
            is_open_ = false;
            return;
        }
    }
    else
    {
        std::cerr << "Unsupported file extension. Supported formats: .aedat4, .raw, .dat" << std::endl;
        is_open_ = false;
        return;
    }


    init();
}

DataSource::DataSource(SDL_GPUDevice* gpu_device, const MetavisionEventReader::LiveCamera& camera)
	: gpu_device(gpu_device), 
    transfer_buffer(gpu_device), 
    name(camera.serial.empty() ? std::string("Prophesee (first available)") : camera.serial), 
    type(Type::CAMERA), 
    is_open_(true), 
    scrubber(gpu_device)
{
    try
    {
        reader = std::make_unique<MetavisionEventReader>(camera);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Prophesee camera reader initialization error: " << e.what() << std::endl;
        is_open_ = false;
        return;
    }

    init();
}

DataSource::DataSource(SDL_GPUDevice* gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera)
	: gpu_device(gpu_device),
    transfer_buffer(gpu_device),
    name(camera.serialNumber), 
    type(Type::CAMERA), 
    is_open_(true),
    scrubber(gpu_device)
{
    // Attempt to initialize camera reader (currently only DVEventReader is supported)
    try
    {
        reader = std::make_unique<DVEventReader>(dv::io::camera::open(camera));
    }
    catch (const std::exception &e)
    {
        // If camera reader initialization fails, log the error and return without initializing render targets
        std::cerr << "Camera reader initialization error: " << e.what() << std::endl;
        is_open_ = false;
        return;
    }

    
    init();
}

DataSource::DataSource(GPUDevice& gpu_device, const ScannedCamera& scanned_camera)
	: gpu_device(gpu_device.get_SDL_device()),
    transfer_buffer(gpu_device.get_SDL_device()), 
    type(Type::CAMERA), 
    is_open_(true),
    scrubber(gpu_device.get_SDL_device())
{

    try {
         if (scanned_camera.vendor == DataSource::Vendor::DV) {
            reader = std::make_unique<DVEventReader>(dv::io::camera::open(scanned_camera.dv_descriptor));
        } else {
            reader = std::make_unique<MetavisionEventReader>(MetavisionEventReader::LiveCamera{scanned_camera.prophesee_serial});
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Camera reader initialization error: " << e.what() << std::endl;
        is_open_ = false;
        return;
    }
   

    init();
}

DataSource::DataSource(GPUDevice& gpu_device, const std::string& file_path): 
    DataSource(gpu_device.get_SDL_device(), file_path) {}

DataSource::DataSource(GPUDevice& gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera): 
    DataSource(gpu_device.get_SDL_device(), camera) {}

DataSource::DataSource(GPUDevice& gpu_device, const MetavisionEventReader::LiveCamera& camera): 
    DataSource(gpu_device.get_SDL_device(), camera) {}

void DataSource::init() {
    // Attempt to read resolution of events
    if (reader->isEventStreamAvailable())
    {
        auto evt_resolution = reader->getEventResolution();
        if (evt_resolution.has_value())
        {
            resolution = cv::Size(evt_resolution->width, evt_resolution->height);
        }
    }
    else {
        std::cerr << "Source does not have an event stream available." << std::endl;
        is_open_ = false;
        return;
    }

    init_render_targets();
}

void DataSource::init_render_targets()
{
    // Read resolution
	std::optional<cv::Size> event_res = reader->getEventResolution();
    resolution = event_res.has_value() ? cv::Size(event_res->width, event_res->height) : cv::Size(1920, 1080);

    // Initialize textures and storage resolution
    dce_render_targets.init_textures(gpu_device, resolution);
    visualizer_render_targets.init_textures(gpu_device, resolution);
    event_data.set_camera_event_resolution(resolution.width, resolution.height);
}

DataSource::~DataSource()
{   
    // Close helper threads
    stop_writing_thread();
    stop_reading_thread();

    // Destory rendering textures 
    dce_render_targets.delete_textures(gpu_device);
    visualizer_render_targets.delete_textures(gpu_device);

    // Clear all event data
    event_data.clear();
}

void DataSource::update()
{
    scrubber.update(event_data, transfer_buffer);
}

bool DataSource::is_open()
{
    return is_open_;
}

size_t DataSource::get_batch_event_data(float event_discard_odds)
{
    // Ensure a reader has been initialized
    if (!reader) return 0;

    // event_discard_odds is the per-event discard probability in [0, 1]
    const bool discard_enabled = event_discard_odds > 0.0f;

    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // Attempt to read data
    size_t events_read = 0;

    try
    {
        if (reader->isEventStreamAvailable())
        {
            if (reader->isEventsRunning()) {
                if (const auto events = reader->getNextEventBatch(); events.has_value())
                {
                    for (const auto &evt : *events)
                    {
                        if (discard_enabled && dist(rng) < event_discard_odds)
                            continue;

                        event_data.write_evt_data(evt);
                        events_read++;
                    }
                }
            } 
            else 
            {
                read_to_end = true;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Event read error: " + std::string(e.what()) << std::endl;
        return events_read; 
    }

    return events_read;
}

size_t DataSource::get_batch_frame_data()
{
    // Ensure a reader has been initialized
    if (!reader) return 0;

    // Attempt to read data
    size_t frames_read = 0;

    try
    {
        if (reader->isFrameStreamAvailable() && reader->isFramesRunning())
        {
            if (const auto frame = reader->getNextFrame(); frame.has_value())
            {
                // Convert BGR (native) to RGB for display
                cv::Mat rgb;
                cv::cvtColor(frame->frameData, rgb, cv::COLOR_BGR2RGB);
                EventData::FrameDatum display_frame{.frameData = rgb.clone(), .timestamp = frame->timestamp};
                event_data.write_frame_data(display_frame);
                
                frames_read++;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Frame read error: " + std::string(e.what()) << std::endl;
        return frames_read;
    }

    return frames_read;
}

void DataSource::reading_loop(float event_discard_odds) {
    while (reading && !read_to_end) {
        get_batch_event_data(event_discard_odds);
        // get_batch_frame_data();
        scrubber.state.update_bounds(event_data);
    }

    reading = false;
}

void DataSource::read(float event_discard_odds, bool blocking) {
    if (reading) return;

    reading = true;
    if (blocking) {
        reading_loop(event_discard_odds);
    } else {
        reading_thread = std::thread([this, event_discard_odds]() {this->reading_loop(event_discard_odds); });
    }
}

void DataSource::stop_reading_thread() {
    reading = false;
    if (reading_thread.joinable()) {
        reading_thread.join();
    }
}

void DataSource::wait_reading_thread() {
    if (reading_thread.joinable()) {
        reading_thread.join();
    }
}

cv::Mat DataSource::save_dce_output() {
    return texture_to_cvmat(dce_render_targets.output.texture, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, resolution.width, resolution.height);
}

cv::Mat DataSource::save_visualizer_output() {
    return texture_to_cvmat(visualizer_render_targets.color.texture, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM, 1920, 1080);
}

cv::Mat DataSource::texture_to_cvmat(SDL_GPUTexture* texture, SDL_GPUTextureFormat texture_format, int width, int height) {
    // Upload data to the new buffer
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);

    cv::Mat result = transfer_buffer.download_to_cv_mat(copy_pass, texture, texture_format, width, height);

    SDL_EndGPUCopyPass(copy_pass);

    // Sync and return
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    SDL_WaitForGPUFences(gpu_device, true, &fence, 1);
    SDL_ReleaseGPUFence(gpu_device, fence);
    return result;
}

void DataSource::save_to_file(const std::string& path, bool blocking) {
    save_to_file_by_index(path, 0, event_data.size()-1, blocking);
}

void DataSource::save_to_file_by_time(const std::string& path, float start_time, float end_time, bool blocking) {
    size_t start_index = event_data.get_event_index_from_relative_timestamp(start_time);
    size_t end_index = event_data.get_event_index_from_relative_timestamp(end_time);
    save_to_file_by_index(path, start_index, end_index, blocking);
}

void DataSource::save_to_file_by_index(const std::string& path, size_t start_index, size_t end_index, bool blocking) {
    if (writing) {
        std::cout << "Writer already running, wait for finish before calling save again" << std::endl;
    }

    writing = true;
    if (blocking) {
        event_data.save_to_file(path, start_index, end_index, writing);
    } else {
        writing_thread = std::thread([this, path, start_index, end_index]() {
            this->event_data.save_to_file(path, start_index, end_index, this->writing);
        });
    }
}

void DataSource::stop_writing_thread() {
    writing = false;
    if (writing_thread.joinable()) {
        writing_thread.join();
    }
}		

void DataSource::wait_writing_thread() {
    if (writing_thread.joinable()) {
        writing_thread.join();
    }
}

std::string DataSource::get_name() {
    return name;
}

bool DataSource::is_reading() {
    return reading;
}

bool DataSource::is_writing() {
    return writing;
}

bool DataSource::is_eof() {
    return read_to_end;
}

cv::Size DataSource::get_resolution() {
    return resolution;
}

DataSource::Type DataSource::get_type() {
    return type;
}

size_t DataSource::size() {
    return event_data.size();
}

DataSource::Vendor DataSource::get_vendor() {
    return vendor;
}

std::vector<DataSource::ScannedCamera> DataSource::get_attached_cameras() {
    
    std::vector<DataSource::ScannedCamera> scanned_cameras;

    // Inivation cameras
    const auto discovered_cameras{dv::io::camera::discover()};
    for (const auto &camera : discovered_cameras)
    {
        scanned_cameras.push_back(DataSource::ScannedCamera{DataSource::Vendor::DV, camera, {}});
    }

    // Prophesee cameras
    try
    {
        const auto prophesee_cameras = Metavision::DeviceDiscovery::list_available_sources();
        for (const auto &desc : prophesee_cameras)
        {
            DataSource::ScannedCamera entry{DataSource::Vendor::PROPHESEE, {}, desc.serial_};
            scanned_cameras.push_back(std::move(entry));
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Prophesee discovery failed: " << e.what() << std::endl;
    }
}