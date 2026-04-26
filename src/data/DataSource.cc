
#include "data/DataSource.hh"
#include "data/IEventReader.hh"
#include "data/DVEventReader.hh"
#include "data/MetavisionEventReader.hh"

#include <memory>
#include <optional>
#include <filesystem>
#include <random>

DataSource::DataSource(SDL_GPUDevice* gpu_device, const std::string& file_path)
	: gpu_device(gpu_device), 
    transfer_buffer(gpu_device), 
    type(Type::FILE), state(State::PAUSED), 
    scrubber(gpu_device)
{
	std::unique_lock da_read_write_lock(mutex);
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
            state = State::FAILED_TO_OPEN;
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
            state = State::FAILED_TO_OPEN;
            return;
        }
    }
    else
    {
        std::cerr << "Unsupported file extension. Supported formats: .aedat4, .raw, .dat" << std::endl;
        state = State::FAILED_TO_OPEN;
        return;
    }


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
        std::cerr << "File does not have an event stream available." << std::endl;
        state = State::FAILED_TO_OPEN;
        return;
    }


    // If initialization is successful, initialize render targets
	init_render_targets();
}

DataSource::DataSource(SDL_GPUDevice* gpu_device, const MetavisionEventReader::LiveCamera& camera)
	: gpu_device(gpu_device), 
    transfer_buffer(gpu_device), 
    name(camera.serial.empty() ? std::string("Prophesee (first available)") : camera.serial), 
    type(Type::CAMERA), state(State::PAUSED), scrubber(gpu_device)
{
	std::unique_lock read_write_lock(mutex);

    try
    {
        reader = std::make_unique<MetavisionEventReader>(camera);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Prophesee camera reader initialization error: " << e.what() << std::endl;
        state = State::FAILED_TO_OPEN;
        return;
    }

    if (reader->isEventStreamAvailable())
    {
        auto evt_resolution = reader->getEventResolution();
        if (evt_resolution.has_value())
        {
            resolution = cv::Size(evt_resolution->width, evt_resolution->height);
        }
    }
    else
    {
        std::cerr << "Prophesee camera does not have an event stream available." << std::endl;
        state = State::FAILED_TO_OPEN;
        return;
    }

	init_render_targets();
}

DataSource::DataSource(SDL_GPUDevice* gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera)
	: gpu_device(gpu_device),
    transfer_buffer(gpu_device),
    name(camera.serialNumber), 
    type(Type::CAMERA), 
    state(State::PAUSED), 
    scrubber(gpu_device)
{
	std::unique_lock read_write_lock(mutex);

    // Attempt to initialize camera reader (currently only DVEventReader is supported)
    try
    {
        reader = std::make_unique<DVEventReader>(dv::io::camera::open(camera));
    }
    catch (const std::exception &e)
    {
        // If camera reader initialization fails, log the error and return without initializing render targets
        std::cerr << "Camera reader initialization error: " << e.what() << std::endl;
        state = State::FAILED_TO_OPEN;
        return;
    }

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
        std::cerr << "Camera does not have an event stream available." << std::endl;
        state = State::FAILED_TO_OPEN;
        return;
    }

    // If initialization is successful, initialize render targets
	init_render_targets();
}

DataSource::DataSource(GPUDevice& gpu_device, const std::string& file_path): 
    DataSource(gpu_device.get_SDL_device(), file_path) {}

DataSource::DataSource(GPUDevice& gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera): 
    DataSource(gpu_device.get_SDL_device(), camera) {}

DataSource::DataSource(GPUDevice& gpu_device, const MetavisionEventReader::LiveCamera& camera): 
    DataSource(gpu_device.get_SDL_device(), camera) {}

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
    std::shared_lock read_lock(mutex);
    return state != State::FAILED_TO_OPEN;
}

size_t DataSource::get_batch_event_data()
{
    // reader is being changed here but it could possibility be switched to a shared_lock if it's too slow
    std::unique_lock read_write_lock(mutex);

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
        if (reader->isEventStreamAvailable() && reader->isEventsRunning())
        {
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
    std::unique_lock da_read_write_lock(mutex);

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

void DataSource::read_all() {
    while (reader->isEventsRunning()) {
        get_batch_event_data();
        get_batch_frame_data();
    }

    scrubber.state.update_bounds(event_data);
}

void DataSource::start_reading_thread() {
    if (reading_thread_running) return; 

    reading_thread_running = true;
    reading_thread = std::thread([this]() {
        while (reading_thread_running) {
            get_batch_event_data();
            get_batch_frame_data();
            scrubber.state.update_bounds(event_data);
        }
    });
}

void DataSource::stop_reading_thread() {
    reading_thread_running = false;
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
    SDL_SubmitGPUCommandBuffer(command_buffer);
    SDL_WaitForGPUIdle(gpu_device);
    return result;
}

void DataSource::save_to_file(const std::string& path) {
    save_to_file_by_index(path, 0, event_data.size()-1);
}

void DataSource::save_to_file_by_time(const std::string& path, float start_time, float end_time) {
    size_t start_index = event_data.get_event_index_from_relative_timestamp(start_time);
    size_t end_index = event_data.get_event_index_from_relative_timestamp(end_time);
    save_to_file_by_index(path, start_index, end_index);
}

void DataSource::save_to_file_by_index(const std::string& path, size_t start_index, size_t end_index) {
    if (writing_thread_running) {
        std::cout << "Cannot call 'save' on DataSource currently being saved. Please wait for previous save to finish." << std::endl;
        return; 
    }

    writing_thread_running = true;
    writing_thread = std::thread([this, path, start_index, end_index]() {
        this->event_data.save_to_file(path, start_index, end_index, this->writing_thread_running);
    });
}

void DataSource::stop_writing_thread() {
    writing_thread_running = false;
    if (writing_thread.joinable()) {
        writing_thread.join();
    }
}		

void DataSource::wait_writing_thread() {
    if (writing_thread.joinable()) {
        writing_thread.join();
    }
}