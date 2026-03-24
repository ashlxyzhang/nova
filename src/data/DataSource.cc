
#include "data/DataSource.hh"

DataSource::DataSource(SDL_GPUDevice* gpu_device, const std::string& file_path)
	: gpu_device(gpu_device), name(file_path), type(Type::FILE), state(State::PAUSED), scrubber(gpu_device)
{
	reader = std::make_unique<IEventReader>(file_path);
	init_render_targets();
}


DataSource::DataSource(SDL_GPUDevice* gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera)
	: gpu_device(gpu_device), name(camera.serialNumber), type(Type::CAMERA), state(State::PAUSED), scrubber(gpu_device)
{
	reader = std::make_unique<IEventReader>(camera);
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

	SDL_GPUTextureCreateInfo color_create_info = {
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE,
		.width = resolution.width,
		.height = resolution.height,
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.sample_count = SDL_GPU_SAMPLECOUNT_1,
	};

	render_targets.dce = {SDL_CreateGPUTexture(gpu_device, &color_create_info), color_create_info.width, color_create_info.height};
    render_targets.positive_values_texture = {SDL_CreateGPUTexture(gpu_device, &color_create_info), color_create_info.width, color_create_info.height};
    render_targets.negative_values_texture = {SDL_CreateGPUTexture(gpu_device, &color_create_info), color_create_info.width, color_create_info.height};
	render_targets.visualizer_color = {SDL_CreateGPUTexture(gpu_device, &color_create_info), color_create_info.width, color_create_info.height};
	render_targets.visualizer_depth = {SDL_CreateGPUTexture(gpu_device, &color_create_info), color_create_info.width, color_create_info.height};
}

DataSource::~DataSource()
{
	SDL_ReleaseGPUTexture(gpu_device, render_targets.dce.texture);
	SDL_ReleaseGPUTexture(gpu_device, render_targets.visualizer_color.texture);
	SDL_ReleaseGPUTexture(gpu_device, render_targets.visualizer_depth.texture);
}

void DataSource::update()
{
    scrubber.update(event_data);
}



/*

float randFloat()
{
	return static_cast<float>(rand()) / RAND_MAX;
};

bool DataAcquisition::init_camera_reader()
{
    std::unique_lock da_read_write_lock(mutex);

    // Ensure valid parameters
    if (scanned_cameras.empty() || camera_index < 0 || camera_index >= scanned_cameras.size())
        return false;

    try
    {
        data_reader_ptr = std::make_unique<DVEventReader>(dv::io::camera::open(scanned_cameras[camera_index]));
    }
    catch (const std::exception &e)
    {
        error_queue.push_error("Camera reader error: " + std::string(e.what()));
        return false;
    }

    if (data_reader_ptr->isEventStreamAvailable())
    {
        auto evt_resolution = data_reader_ptr->getEventResolution();
        if (evt_resolution.has_value())
        {
            camera_event_width = evt_resolution->width;
            camera_event_height = evt_resolution->height;
        }
    }
    if (data_reader_ptr->isFrameStreamAvailable())
    {
        auto frame_resolution = data_reader_ptr->getFrameResolution();
        if (frame_resolution.has_value())
        {
            camera_frame_width = frame_resolution->width;
            camera_frame_height = frame_resolution->height;
        }
    }

    return true;
}

bool DataAcquisition::init_file_reader()
{
    std::unique_lock da_read_write_lock(mutex);

    // Check for file extension
    size_t extension_pos = file_stream_name.find_last_of('.');
    if (extension_pos == std::string::npos)
    {
        error_queue.push_error("File has no extension!");

        return false;
    }

    // Check for valid file extension
    std::string ext = file_stream_name.substr(extension_pos);
    if (ext == ".aedat4")
    {
        try
        {
            data_reader_ptr =
                std::make_unique<DVEventReader>(std::make_unique<dv::io::MonoCameraRecording>(file_stream_name));
        }
        catch (const std::exception &e)
        {
            error_queue.push_error("aedat4 reader error: " + std::string(e.what()));
            return false;
        }
    }
    else if (ext == ".raw" || ext == ".dat")
    {
        try
        {
            data_reader_ptr = std::make_unique<MetavisionEventReader>(file_stream_name);
        }
        catch (const std::exception &e)
        {
            error_queue.push_error("Metavision reader error: " + std::string(e.what()));
            return false;
        }
    }
    else
    {
        error_queue.push_error("Unsupported file extension. Supported formats: .aedat4, .raw, .dat");
        return false;
    }

    // Read the event-resolution of the targeted input
    if (data_reader_ptr->isEventStreamAvailable())
    {
        auto evt_resolution = data_reader_ptr->getEventResolution();
        if (evt_resolution.has_value())
        {
            camera_event_width = evt_resolution->width;
            camera_event_height = evt_resolution->height;
        }
    }
    if (data_reader_ptr->isFrameStreamAvailable())
    {
        auto frame_resolution = data_reader_ptr->getFrameResolution();
        if (frame_resolution.has_value())
        {
            camera_frame_width = frame_resolution->width;
            camera_frame_height = frame_resolution->height;
        }
    }

    file_stream_changed = false;
    return true;
}

bool DataAcquisition::get_batch_evt_data(EventData &evt_data, DataWriter &data_writer)
{
    // data_reader_ptr is being changed here but it could possibility be switched to a shared_lock if it's too slow
    std::unique_lock da_read_write_lock(mutex);

    // Ensure a reader has been initialized
    if (!data_reader_ptr)
        return false;

    // Calculate drop out threshold
    if (event_discard_odds < 0.00001)
    {
        error_queue.push_error("Event Discard Odds are too low!");
        return false;
    }
    float threshold = 1.0f / event_discard_odds;

    // Attempt to read data
    bool any_data_read = false;
    try
    {
        if (data_reader_ptr->isEventStreamAvailable() && data_reader_ptr->isEventsRunning())
        {
            if (const auto events = data_reader_ptr->getNextEventBatch(); events.has_value())
            {
                dv::EventStore event_store{};
                for (const auto &evt : *events)
                {
                    if (randFloat() > threshold)
                        continue;

                    evt_data.write_evt_data(evt);
                    any_data_read = true;

                    event_store.emplace_back(evt.timestamp, evt.x, evt.y, evt.polarity);
                }

                if (data_writer.get_writing_event_data())
                {
                    data_writer.add_event_store(event_store);
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        error_queue.push_error("Event read error: " + std::string(e.what()));
        return false;
    }

    return any_data_read;
}

bool DataAcquisition::get_batch_frame_data(EventData &evt_data, DataWriter &data_writer)
{
    std::unique_lock da_read_write_lock(mutex);

    // Ensure a reader has been initialized
    if (!data_reader_ptr)
        return false;

    // Attempt to read data
    bool any_data_read = false;
    try
    {
        if (data_reader_ptr->isFrameStreamAvailable() && data_reader_ptr->isFramesRunning())
        {
            if (const auto frame = data_reader_ptr->getNextFrame(); frame.has_value())
            {
                // Convert BGR (native) to RGB for display
                cv::Mat rgb;
                cv::cvtColor(frame->frameData, rgb, cv::COLOR_BGR2RGB);
                EventData::FrameDatum display_frame{.frameData = rgb.clone(), .timestamp = frame->timestamp};
                evt_data.write_frame_data(display_frame);
                any_data_read = true;

                // Write original BGR image to file
                if (data_writer.get_writing_frame_data())
                {
                    dv::Frame dv_frame(frame->timestamp, frame->frameData);
                    data_writer.add_frame_data(dv_frame);
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        error_queue.push_error("Frame read error: " + std::string(e.what()));
        return false;
    }

    return any_data_read;
}

*/