
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
	std::optional<cv::Size> event_res = reader->getEventResolution();
	if (event_res.has_value()) 
	{
		resolution = cv::Size(event_res->width, event_res->height);
	}
	else
	{
		resolution = cv::Size(1920, 1080);
	}

	SDL_GPUTextureCreateInfo dce_create_info = {
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE,
		.width = (Uint32) resolution.width,
		.height = (Uint32) resolution.height,
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.sample_count = SDL_GPU_SAMPLECOUNT_1,
	};
    render_targets.dce = {SDL_CreateGPUTexture(gpu_device, &dce_create_info), dce_create_info.width, dce_create_info.height};

    SDL_GPUTextureCreateInfo dce_intermediate_create_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R32_UINT,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE |
                    SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE,
        .width = (Uint32) resolution.width,
        .height = (Uint32) resolution.height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
    };
    render_targets.positive_values_texture = {SDL_CreateGPUTexture(gpu_device, &dce_intermediate_create_info), dce_intermediate_create_info.width, dce_intermediate_create_info.height};
    render_targets.negative_values_texture = {SDL_CreateGPUTexture(gpu_device, &dce_intermediate_create_info), dce_intermediate_create_info.width, dce_intermediate_create_info.height};
    

    SDL_GPUTextureCreateInfo vis_color_create_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = 1920,
        .height = 1200,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
    };
	render_targets.visualizer_color = {SDL_CreateGPUTexture(gpu_device, &vis_color_create_info), vis_color_create_info.width, vis_color_create_info.height};
	
    SDL_GPUTextureCreateInfo vis_depth_create_info = {
        .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .width = 1920,
        .height = 1200,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
    };
    render_targets.visualizer_depth = {SDL_CreateGPUTexture(gpu_device, &vis_depth_create_info), vis_depth_create_info.width, vis_depth_create_info.height};
    event_data.set_camera_event_resolution(resolution.width, resolution.height);
}

DataSource::~DataSource()
{
	SDL_ReleaseGPUTexture(gpu_device, render_targets.dce.texture);
	SDL_ReleaseGPUTexture(gpu_device, render_targets.positive_values_texture.texture);
	SDL_ReleaseGPUTexture(gpu_device, render_targets.negative_values_texture.texture);
    SDL_ReleaseGPUTexture(gpu_device, render_targets.visualizer_color.texture);
    SDL_ReleaseGPUTexture(gpu_device, render_targets.visualizer_depth.texture);
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

