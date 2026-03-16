#include "DataWriter.hh"
#include "DataAcquisition.hh"

void DataWriter::set_save_frames_toggle(const bool toggle) {
    std::unique_lock lock(mutex);
    save_frames_toggle = toggle;
}

bool DataWriter::get_save_frames_toggle() {
    std::shared_lock lock(mutex);
    return save_frames_toggle;
}

void DataWriter::set_save_events_toggle(const bool toggle) {
    std::unique_lock lock(mutex);
    save_events_toggle = toggle;
}

bool DataWriter::get_save_events_toggle() {
    std::shared_lock lock(mutex);
    return save_events_toggle;
}

void DataWriter::set_saving_message(const std::string& message) {
    std::unique_lock lock(mutex);
    saving_message = message;
}

std::string DataWriter::get_saving_message() {
    std::shared_lock lock(mutex);
    return saving_message;
}

void DataWriter::set_stream_save_file_name(const std::string &file_name) {
    std::unique_lock lock(mutex);
    stream_save_file_name = file_name;
}

std::string DataWriter::get_stream_save_file_name() {
    std::shared_lock lock(mutex);
    return stream_save_file_name;
}

bool DataWriter::get_writing_frame_data()
{
    std::shared_lock lock(mutex);
    return writing_frame_data;
}

bool DataWriter::get_writing_event_data()
{
    std::shared_lock lock(mutex);
    return writing_event_data;
}

DataWriter::DataWriter(ErrorQueue &error_queue): error_queue(error_queue) {}

void DataWriter::setup(DataAcquisition &data_acq)
{  
    clear(); // clear() also uses lock so make sure this is before the new lock is created
    std::unique_lock dw_read_write_lock(mutex);
    // Ensure the output file has a valid type
    if (!stream_save_file_name.ends_with(".aedat4")) stream_save_file_name.append(".aedat4");

    // If we are reading from a file, make sure the output file doesn't have the same name as the input file
    if (data_acq.get_state() == DataAcquisition::STATE::FILE_STREAM)
    {
        if (stream_save_file_name == data_acq.get_file_stream_name())
        {
            size_t aedat_index = stream_save_file_name.find(".aedat4");
            stream_save_file_name.insert(aedat_index, "new");
        }
    }

    // Check whether the user has toggled to save events and/or frames to the output file
    if (save_events_toggle || save_frames_toggle)
    {                  
        // Attempt to initialize the writer
        if (init_data_writer(data_acq))
        {  

            // Compose a message to tell user what is being saved
            std::string saving_message = "Currently Saving ";

            if (save_events_toggle)
            {
                saving_message.append("Event Data ");
            }
            if (save_frames_toggle)
            {
                saving_message.append(save_events_toggle ? "And Frame Data " : "Frame Data ");
            }
            saving_message.append("To ");
            saving_message.append(stream_save_file_name);

            // Reset saving controls
            stream_save_file_name = "";
            save_events_toggle = false;
            save_frames_toggle = false;

        }
        else
        {  
            // If initialization failed, just clear everything
            error_queue.push_error("Data Reader Initialization failed for some god damn reason 🤷");
            dw_read_write_lock.unlock();
            clear(); // clear() requires the lock so you need to unlock prior
        }
    }
}

void DataWriter::clear()
{
    std::unique_lock dw_read_write_lock(mutex);
    data_writer_ptr.reset();

    // Clear states
    writing_frame_data = false;
    writing_event_data = false;

    // Clear queues
    while (!writer_event_queue.empty())
    {
        writer_event_queue.pop();
    }

    while (!writer_frame_queue.empty())
    {
        writer_frame_queue.pop();
    }

    saving_message = "Nothing Being Saved Currently";
}

bool DataWriter::init_data_writer(DataAcquisition &data_acq)
{
    // Create config for writing all types of data (event, frame, IMU) for DAVIS Camera
    // https://dv-processing.inivation.com/131-add-wengen-to-dv-processing-2-0/writing_data.html
    try
    {
        dv::io::MonoCameraWriter::Config writer_config("Save Config");

        cv::Size file_res((std::max)(data_acq.get_camera_event_width(), data_acq.get_camera_frame_width()),
                          (std::max)(data_acq.get_camera_event_height(), data_acq.get_camera_frame_height()));

        writing_event_data = save_events_toggle;
        writing_frame_data = save_frames_toggle;

        if (save_events_toggle)
        {
            writer_config.addEventStream(file_res);
        }

        if (save_frames_toggle)
        {
            writer_config.addFrameStream(file_res);
        }

        data_writer_ptr = std::make_unique<dv::io::MonoCameraWriter>(stream_save_file_name, writer_config);
    }
    catch (...)
    {
        error_queue.push_error("Something went wrong initializing file to save to!");
        return false;
    }

    return true;
}

void DataWriter::add_event_store(dv::EventStore evt_store)
{
    std::unique_lock dw_read_write_lock(mutex);
    writer_event_queue.push(evt_store);
}

void DataWriter::add_frame_data(dv::Frame frame_data)
{
    std::unique_lock dw_read_write_lock(mutex);
    writer_frame_queue.push(frame_data);
}

bool DataWriter::write_event_store()
{
    std::unique_lock dw_read_write_lock(mutex);

    if (writer_event_queue.empty() || !data_writer_ptr || !writing_event_data) return false;
    dv::EventStore evt_store{writer_event_queue.front()};
    writer_event_queue.pop();

    try
    {
        data_writer_ptr->writeEvents(evt_store);
    }
    catch (...)
    {
        error_queue.push_error("Something went wrong with saving event data!");
        return false;
    }

    return true;
}

bool DataWriter::write_frame_data()
{
    std::unique_lock dw_read_write_lock(mutex);

    if (writer_frame_queue.empty() || !data_writer_ptr || !writing_frame_data) return false;

    dv::Frame frame_data{writer_frame_queue.front()};
    writer_frame_queue.pop();

    try
    {
        data_writer_ptr->writeFrame(frame_data);
    }
    catch (...)
    {
        error_queue.push_error("Something went wrong with saving frame data!");
        return false;
    }

    return true;
}