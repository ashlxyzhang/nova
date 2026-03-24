#include "data/DataAcquisition.hh"
#include "data/DataWriter.hh"

DataAcquisition::DataAcquisition(ErrorQueue &error_queue)
    : error_queue(error_queue), data_reader_ptr{}, camera_event_width{}, camera_event_height{}, camera_frame_width{},
      camera_frame_height{}
{
}

void DataAcquisition::clear()
{
    std::unique_lock da_read_write_lock(mutex);

    data_reader_ptr.reset();

    camera_event_width = 0;
    camera_event_height = 0;
    camera_frame_width = 0;
    camera_frame_height = 0;

    file_stream_paused = false;
    file_stream_changed = false;
    camera_stream_paused = false;
    camera_stream_changed = false;
}

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

void DataAcquisition::get_camera_event_resolution(EventData &evt_data)
{
    evt_data.set_camera_event_resolution(camera_event_width, camera_event_height);
}

void DataAcquisition::get_camera_frame_resolution(EventData &evt_data)
{
    evt_data.set_camera_frame_resolution(camera_frame_width, camera_frame_height);
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

// Thread-safe getters
DataAcquisition::STATE DataAcquisition::get_state()
{
    std::shared_lock lock(mutex);
    return state;
}
float DataAcquisition::get_event_discard_odds()
{
    std::shared_lock lock(mutex);
    return event_discard_odds;
}
int32_t DataAcquisition::get_camera_index()
{
    std::shared_lock lock(mutex);
    return camera_index;
}
bool DataAcquisition::is_camera_stream_paused()
{
    std::shared_lock lock(mutex);
    return camera_stream_paused;
}
bool DataAcquisition::has_camera_stream_changed()
{
    std::shared_lock lock(mutex);
    return camera_stream_changed;
}
std::string DataAcquisition::get_file_stream_name()
{
    std::shared_lock lock(mutex);
    return file_stream_name;
}
bool DataAcquisition::is_file_stream_paused()
{
    std::shared_lock lock(mutex);
    return file_stream_paused;
}
bool DataAcquisition::has_file_stream_changed()
{
    std::shared_lock lock(mutex);
    return file_stream_changed;
}
std::vector<std::string> DataAcquisition::get_scanned_camera_names()
{
    std::shared_lock lock(mutex);
    return scanned_camera_names;
}
int32_t DataAcquisition::get_camera_event_width()
{
    std::shared_lock da_read_lock(mutex);
    return camera_event_width;
}
int32_t DataAcquisition::get_camera_frame_width()
{
    std::shared_lock da_read_lock(mutex);
    return camera_frame_width;
}
int32_t DataAcquisition::get_camera_event_height()
{
    std::shared_lock da_read_lock(mutex);
    return camera_event_height;
}
int32_t DataAcquisition::get_camera_frame_height()
{
    std::shared_lock da_read_lock(mutex);
    return camera_frame_height;
}

// Thread-safe setters
void DataAcquisition::set_event_discard_odds(float odds)
{
    std::unique_lock lock(mutex);
    event_discard_odds = odds;
}
void DataAcquisition::set_camera_index(int32_t index)
{
    std::unique_lock lock(mutex);
    camera_index = index;
    camera_stream_changed = true;
}
void DataAcquisition::set_camera_stream_paused(bool paused)
{
    std::unique_lock lock(mutex);
    camera_stream_paused = paused;
}
void DataAcquisition::set_camera_stream_changed(bool changed)
{
    std::unique_lock lock(mutex);
    camera_stream_changed = changed;
}
void DataAcquisition::set_file_stream_name(const std::string &name)
{
    std::unique_lock lock(mutex);
    file_stream_name = name;
    file_stream_changed = true;
}
void DataAcquisition::set_file_stream_paused(bool paused)
{
    std::unique_lock lock(mutex);
    file_stream_paused = paused;
}
void DataAcquisition::set_file_stream_changed(bool changed)
{
    std::unique_lock lock(mutex);
    file_stream_changed = changed;
}
void DataAcquisition::set_state(const STATE new_state)
{
    std::unique_lock lock(mutex);
    state = new_state;
}
