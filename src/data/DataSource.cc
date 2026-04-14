
#include "data/DataSource.hh"
#include "data/IEventReader.hh"
#include "data/DVEventReader.hh"
#include "data/MetavisionEventReader.hh"

#include <memory>
#include <optional>
#include <filesystem>

DataSource::DataSource(SDL_GPUDevice* gpu_device, const std::string& file_path)
	: gpu_device(gpu_device), type(Type::FILE), state(State::PAUSED), scrubber(gpu_device)
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
	: gpu_device(gpu_device), name(camera.serial.empty() ? std::string("Prophesee (first available)") : camera.serial),
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
	: gpu_device(gpu_device), name(camera.serialNumber), type(Type::CAMERA), state(State::PAUSED), scrubber(gpu_device)
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
    dce_render_targets.delete_textures(gpu_device);
    visualizer_render_targets.delete_textures(gpu_device);
    event_data.clear();
}

void DataSource::update()
{
    scrubber.update(event_data);
}

bool DataSource::is_open()
{
    std::shared_lock read_lock(mutex);
    return state != State::FAILED_TO_OPEN;
}

void DataSource::get_batch_event_data()
{
    // reader is being changed here but it could possibility be switched to a shared_lock if it's too slow
    std::unique_lock read_write_lock(mutex);

    // Ensure a reader has been initialized
    if (!reader) return;

    // Calculate drop out threshold
    float threshold = 1.0f / event_discard_odds;

    // Attempt to read data
    try
    {
        if (reader->isEventStreamAvailable() && reader->isEventsRunning())
        {
            if (const auto events = reader->getNextEventBatch(); events.has_value())
            {
                for (const auto &evt : *events)
                {
                    if (static_cast<float>(rand()) / RAND_MAX > threshold)
                        continue;

                    event_data.write_evt_data(evt);
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Event read error: " + std::string(e.what()) << std::endl;
        return;
    }
}

void DataSource::get_batch_frame_data()
{
    std::unique_lock da_read_write_lock(mutex);

    // Ensure a reader has been initialized
    if (!reader) return;

    // Attempt to read data
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
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Frame read error: " + std::string(e.what()) << std::endl;
        return;
    }
}

