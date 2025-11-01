#include "DataAcquisition.hh"
#include "DataWriter.hh"
#include "EventData.hh"
#include "ParameterStore.hh"
#include "GUI.hh"

/**
 * @brief Thread for writing data back to persistent storage when streaming.
 * @param running Atomic boolean that determines if thread is running or not.
 * @param data_writer DataWriter object to write data with
 *
 */

inline void writer_thread(std::atomic<bool> &running, DataWriter &data_writer)
{
    // For now let us spin
    while (running)
    {
        data_writer.write_event_store();
        data_writer.write_frame_data();
    }
}

/**
 * @brief Thread for data acquisition, storing into event_data
 * @param running Atomic boolean that determines if thread is running or not.
 * @param param_store ParameterStore object to get GUI info from.
 * @param evt_data EventData object to store event/frame data into for drawing.
 * @param data_writer DataWriter object to store event/frame data into to be saved to persistent storage.
 */
// Thread for data acquisition, storing into event_data
inline void data_acquisition_thread(std::atomic<bool> &running, DataAcquisition &data_acq, ParameterStore &param_store,
                                    EventData &evt_data, DataWriter &data_writer)
{
    while (running)
    {
        // DATA ACQUISITION CODE
        if(param_store.exists("program_state"))
        {
            
            GUI::PROGRAM_STATE prog_state{static_cast<GUI::PROGRAM_STATE>(param_store.get<uint8_t>("program_state"))};
            switch(prog_state)
            {
                case GUI::PROGRAM_STATE::FILE_READ: // Case for reading from file
                    if (param_store.exists("load_file_name") && param_store.exists("load_file_changed"))
                    {
                        if (param_store.get<bool>("load_file_changed"))
                        {
                            std::string load_file_name{param_store.get<std::string>("load_file_name")};

                            evt_data.clear();
                            data_writer.clear();
                            bool init_success{data_acq.init_file_reader(load_file_name, param_store)};

                            if (init_success)
                            {
                                data_acq.get_camera_event_resolution(evt_data);
                                data_acq.get_camera_frame_resolution(evt_data);
                                data_acq.get_all_evt_data(evt_data, param_store, data_writer);
                                data_acq.get_all_frame_data(evt_data, param_store, data_writer);
                                param_store.add("load_file_changed", false);
                                param_store.add("resolution_initialized", true); // Need to communicate with DCE

                                // Test to ensure event/frame data was added and is ordered
                                // evt_data.lock_data_vectors();

                                // const auto &event_data{evt_data.get_evt_vector_ref()};

                                // for (size_t i = 1; i < event_data.size(); ++i)
                                // {
                                //     assert(event_data[i - 1][2] <= event_data[i][2]); // Ensure ascending timestamps
                                // }

                                // const auto &frame_data{evt_data.get_frame_vector_ref(true)};

                                // for (size_t i = 1; i < frame_data.size(); ++i)
                                // {
                                //     assert(frame_data[i].second <= frame_data[i].second); // Ensure ascending timestamps
                                // }

                                // evt_data.unlock_data_vectors();
                            }
                        }
                    }
                    break;
                
                case GUI::PROGRAM_STATE::FILE_STREAM: // Case for streaming from file
                    if (param_store.exists("stream_file_name") && param_store.exists("stream_file_changed") &&
                            param_store.exists("stream_paused"))
                    {
                        // If stream file changed, reset reader to read from new file and clear previously read event data
                        if (param_store.get<bool>("stream_file_changed"))
                        {
                            std::string stream_file_name{param_store.get<std::string>("stream_file_name")};
                            evt_data.clear();
                            bool init_success{data_acq.init_file_reader(stream_file_name, param_store)};
                            if (init_success)
                            {
                                data_acq.get_camera_event_resolution(evt_data);
                                data_acq.get_camera_frame_resolution(evt_data);
                                param_store.add("stream_file_changed", false);
                                param_store.add("resolution_initialized", true); // Need to communicate with DCE
                            }

                            // If gui indicates writing needs to be done, then set up writer for writing
                            data_writer.clear();
                            if (param_store.exists("stream_save") && param_store.exists("stream_save_file_name") &&
                                param_store.get<bool>("stream_save"))
                            {
                                std::string stream_save_file_name{param_store.get<std::string>("stream_save_file_name")};
                                std::string stream_file_name{param_store.get<std::string>("stream_file_name")};

                                if(!stream_save_file_name.ends_with(".aedat4"))
                                {
                                    stream_save_file_name.append(".aedat4");
                                }

                                if(!stream_file_name.ends_with(".aedat4"))
                                {
                                    stream_file_name.append(".aedat4");
                                }
                                std::cout << "STREAM FILE: " << stream_file_name << "STREAM SAVE: " << stream_save_file_name << std::endl;
                                // Attempting to write to file while reading from it will lead to disaster
                                if(stream_save_file_name == stream_file_name)   
                                {
                                    stream_save_file_name.append("new"); // Append new to ensure different name
                                    param_store.add("stream_save_file_name", stream_save_file_name);
                                }

                                data_writer.init_data_writer(
                                    stream_save_file_name, data_acq.get_camera_event_width(),
                                    data_acq.get_camera_event_height(), data_acq.get_camera_frame_width(),
                                    data_acq.get_camera_frame_height());

                                
                            }
                        }

                        // Check if stream is paused
                        bool stream_paused{param_store.get<bool>("stream_paused")};
                        if (!stream_paused)
                        {
                            // Get event/frame data in batches every frame
                            data_acq.get_batch_evt_data(evt_data, param_store, data_writer);
                            data_acq.get_batch_frame_data(evt_data, param_store, data_writer);

                            // Test to ensure event/frame data was added and is ordered
                            // evt_data.lock_data_vectors();

                            // const auto &event_data{evt_data.get_evt_vector_ref()};

                            // for (size_t i = 1; i < event_data.size(); ++i)
                            // {
                            //     assert(event_data[i - 1][2] <= event_data[i][2]); // Ensure ascending timestamps
                            //     // std::cout << "AT i: " << i << " INDEX: " <<
                            //     // evt_data.get_index_from_timestamp(event_data[i][2]) << std::endl;
                            // }

                            // const auto &frame_data{evt_data.get_frame_vector_ref()};
                            // std::cout << "FRAME DATA RECEIVED, SIZE: " << frame_data.size() << std::endl;

                            // for (size_t i = 1; i < frame_data.size(); ++i)
                            // {
                            //     std::cout << "AT i: " << i << " TIMESTAMP: " << frame_data[i].second << std::endl;
                            //     assert(frame_data[i].second <= frame_data[i].second); // Ensure ascending timestamps
                            //     std::cout << "RETRIEVED: " << evt_data.get_frame_index_from_timestamp(frame_data[i].second)
                            //     << std::endl;
                            // }

                            // evt_data.unlock_data_vectors();
                        }
                    }
                    break;
                case GUI::PROGRAM_STATE::CAMERA_STREAM: // Case for streaming from camera
                    if(param_store.exists("start_camera_scan") && param_store.exists("camera_index") && param_store.exists("camera_changed") && param_store.exists("camera_stream_paused"))
                    {
                        if(param_store.get<bool>("start_camera_scan"))
                        {
                            data_acq.discover_cameras(param_store);
                            param_store.add("start_camera_scan", false);
                        }

                        if(param_store.get<bool>("camera_changed"))
                        {
                            bool init_success{data_acq.init_camera_reader(param_store.get<int32_t>("camera_index"), param_store)};

                            if(init_success)
                            {
                                data_acq.get_camera_event_resolution(evt_data);
                                data_acq.get_camera_frame_resolution(evt_data);
                                param_store.add("camera_changed", false);
                                param_store.add("resolution_initialized", true); // Need to communicate with DCE
                            }

                            // If gui indicates writing needs to be done, then set up writer for writing
                            data_writer.clear();
                            if (param_store.exists("stream_save") && param_store.exists("stream_save_file_name") &&
                                param_store.get<bool>("stream_save"))
                            {
                                std::string stream_save_file_name{param_store.get<std::string>("stream_save_file_name")};
                                std::string stream_file_name{param_store.get<std::string>("stream_file_name")};

                                if(!stream_save_file_name.ends_with(".aedat4"))
                                {
                                    stream_save_file_name.append(".aedat4");
                                }

                                if(!stream_file_name.ends_with(".aedat4"))
                                {
                                    stream_file_name.append(".aedat4");
                                }
                                std::cout << "STREAM FILE: " << stream_file_name << "STREAM SAVE: " << stream_save_file_name << std::endl;
                                // Attempting to write to file while reading from it will lead to disaster
                                if(stream_save_file_name == stream_file_name)   
                                {
                                    stream_save_file_name.append("new"); // Append new to ensure different name
                                    param_store.add("stream_save_file_name", stream_save_file_name);
                                }

                                data_writer.init_data_writer(
                                    stream_save_file_name, data_acq.get_camera_event_width(),
                                    data_acq.get_camera_event_height(), data_acq.get_camera_frame_width(),
                                    data_acq.get_camera_frame_height());

                                
                            }

                        }

                        // Check if stream is paused
                        bool camera_stream_paused{param_store.get<bool>("camera_stream_paused")};
                        if (!camera_stream_paused)
                        {
                            // Get event/frame data in batches every frame
                            data_acq.get_batch_evt_data(evt_data, param_store, data_writer);
                            data_acq.get_batch_frame_data(evt_data, param_store, data_writer);

                            // Test to ensure event/frame data was added and is ordered
                            // evt_data.lock_data_vectors();

                            // const auto &event_data{evt_data.get_evt_vector_ref()};

                            // for (size_t i = 1; i < event_data.size(); ++i)
                            // {
                            //     assert(event_data[i - 1][2] <= event_data[i][2]); // Ensure ascending timestamps
                            //     // std::cout << "AT i: " << i << " INDEX: " <<
                            //     // evt_data.get_index_from_timestamp(event_data[i][2]) << std::endl;
                            // }

                            // const auto &frame_data{evt_data.get_frame_vector_ref()};
                            // std::cout << "FRAME DATA RECEIVED, SIZE: " << frame_data.size() << std::endl;

                            // for (size_t i = 1; i < frame_data.size(); ++i)
                            // {
                            //     std::cout << "AT i: " << i << " TIMESTAMP: " << frame_data[i].second << std::endl;
                            //     assert(frame_data[i].second <= frame_data[i].second); // Ensure ascending timestamps
                            //     std::cout << "RETRIEVED: " << evt_data.get_frame_index_from_timestamp(frame_data[i].second)
                            //     << std::endl;
                            // }

                            // evt_data.unlock_data_vectors();
                        }


                    }
                default:
                    break;
                    
            }
        }
    
    }
}

