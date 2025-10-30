#include "pch.hh"

#include "DataAcquisition.hh"
#include "DataWriter.hh"
#include "GUI.hh"
#include "ParameterStore.hh"
#include "RenderTarget.hh"
#include "Scrubber.hh"
#include "UploadBuffer.hh"
#include "Visualizer.hh"

ParameterStore *g_parameter_store = nullptr;

SDL_Window *g_window = nullptr;
SDL_GPUDevice *g_gpu_device = nullptr;

UploadBuffer *g_upload_buffer = nullptr;

GUI *g_gui = nullptr;
Scrubber *g_scrubber = nullptr;
Visualizer *g_visualizer = nullptr;

std::unordered_map<std::string, RenderTarget> g_render_targets;

float g_last_frame_render_time{0.0f};

EventData g_event_data{};
DataAcquisition g_data_acq{};
DataWriter g_data_writer{};

// Needed to ensure thread joins for program clean up
std::atomic<bool> g_writer_running{true};

// Thread for writing data back to persistent storage when streaming
inline void writer_thread()
{
    // For now let us spin
    while (g_writer_running)
    {
        g_data_writer.write_event_store();
        g_data_writer.write_frame_data();
    }
}

// Start writer thread
std::shared_ptr<std::thread> g_writer_thread_obj;

// Needed to ensure thread joins for program clean up
std::atomic<bool> g_data_acquisition_running{true};

// Thread for data acquisition, storing into g_event_data
inline void data_acquisition_thread()
{
    while (g_data_acquisition_running)
    {
        // DATA ACQUISITION CODE
        if (g_parameter_store->exists("streaming"))
        {
            bool streaming{g_parameter_store->get<bool>("streaming")};
            // Case for not streaming
            if (!streaming && g_parameter_store->exists("load_file_name") &&
                g_parameter_store->exists("load_file_changed"))
            {
                if (g_parameter_store->get<bool>("load_file_changed"))
                {
                    std::string load_file_name{g_parameter_store->get<std::string>("load_file_name")};

                    g_event_data.clear();
                    g_data_writer.clear();
                    bool init_success{g_data_acq.init_reader(load_file_name)};

                    if (init_success)
                    {
                        g_data_acq.get_camera_resolution(g_event_data);
                        g_data_acq.get_all_evt_data(g_event_data, *g_parameter_store, g_data_writer);
                        g_data_acq.get_all_frame_data(g_event_data, *g_parameter_store, g_data_writer);
                        g_parameter_store->add("load_file_changed", false);

                        // Test to ensure event/frame data was added and is ordered
                        g_event_data.lock_data_vectors();

                        const auto &event_data{g_event_data.get_evt_vector_ref(true)};

                        // for (size_t i = 1; i < event_data.size(); ++i)
                        // {
                        //     assert(event_data[i - 1][2] <= event_data[i][2]); // Ensure ascending timestamps
                        // }

                        // const auto &frame_data{g_event_data.get_frame_vector_ref(true)};

                        // for (size_t i = 1; i < frame_data.size(); ++i)
                        // {
                        //     assert(frame_data[i].second <= frame_data[i].second); // Ensure ascending timestamps
                        // }

                        g_event_data.unlock_data_vectors();
                    }
                }
            }
            // case for streaming
            else if (streaming && g_parameter_store->exists("stream_file_name") &&
                     g_parameter_store->exists("stream_file_changed") && g_parameter_store->exists("stream_paused"))
            {
                // If stream file changed, reset reader to read from new file and clear previously read event data
                if (g_parameter_store->get<bool>("stream_file_changed"))
                {
                    std::string stream_file_name{g_parameter_store->get<std::string>("stream_file_name")};
                    g_event_data.clear();
                    bool init_success{g_data_acq.init_reader(stream_file_name)};
                    if (init_success)
                    {
                        g_data_acq.get_camera_resolution(g_event_data);
                        g_parameter_store->add("stream_file_changed", false);
                    }

                    // If gui indicates writing needs to be done, then set up writer for writing
                    g_data_writer.clear();
                    if (g_parameter_store->exists("stream_save") &&
                        g_parameter_store->exists("stream_save_file_name") &&
                        g_parameter_store->get<bool>("stream_save"))
                    {
                        g_data_writer.init_data_writer(g_parameter_store->get<std::string>("stream_save_file_name"),
                                                       g_data_acq.get_camera_height(), g_data_acq.get_camera_width());
                    }
                }

                // Check if stream is paused
                bool stream_paused{g_parameter_store->get<bool>("stream_paused")};
                if (!stream_paused)
                {
                    // Get event/frame data in batches every frame
                    g_data_acq.get_batch_evt_data(g_event_data, *g_parameter_store, g_data_writer);
                    g_data_acq.get_batch_frame_data(g_event_data, *g_parameter_store, g_data_writer);

                    // Test to ensure event/frame data was added and is ordered
                    g_event_data.lock_data_vectors();

                    const auto &event_data{g_event_data.get_evt_vector_ref(true)};

                    // for (size_t i = 1; i < event_data.size(); ++i)
                    // {
                    //     assert(event_data[i - 1][2] <= event_data[i][2]); // Ensure ascending timestamps
                    //     // std::cout << "AT i: " << i << " INDEX: " <<
                    //     // g_event_data.get_index_from_timestamp(event_data[i][2]) << std::endl;
                    // }

                    // const auto &frame_data{g_event_data.get_frame_vector_ref(true)};
                    // // std::cout << "FRAME DATA RECEIVED, SIZE: " << frame_data.size() << std::endl;

                    // for (size_t i = 1; i < frame_data.size(); ++i)
                    // {
                    //     assert(frame_data[i].second <= frame_data[i].second); // Ensure ascending timestamps
                    // }

                    g_event_data.unlock_data_vectors();
                }
            }
        }
    }
}

std::shared_ptr<std::thread> g_data_acquisition_thread_obj;

// This function runs once at startup.
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    g_parameter_store = new ParameterStore();

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create SDL window
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    g_window = SDL_CreateWindow("Nova", 1280, 720, window_flags);
    if (g_window == nullptr)
    {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create GPU Device
    g_gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");
    if (g_gpu_device == nullptr)
    {
        SDL_Log("Couldn't create GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Claim window for GPU Device
    if (!SDL_ClaimWindowForGPUDevice(g_gpu_device, g_window))
    {
        SDL_Log("Couldn't claim window for GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetGPUSwapchainParameters(g_gpu_device, g_window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);

    g_upload_buffer = new UploadBuffer(g_gpu_device);

    // if we have a single use upload, like uploading a static mesh, might as well do it here
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(g_gpu_device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);

    g_scrubber = new Scrubber(*g_parameter_store, &g_event_data, g_gpu_device);
    g_gui = new GUI(g_render_targets, g_parameter_store, g_window, g_gpu_device, g_scrubber);
    g_visualizer = new Visualizer(*g_parameter_store, g_render_targets, g_event_data, g_scrubber, g_window,
                                  g_gpu_device, g_upload_buffer, copy_pass);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);

    // Initialize threads
    g_writer_thread_obj = std::make_shared<std::thread>(writer_thread);
    g_data_acquisition_thread_obj = std::make_shared<std::thread>(data_acquisition_thread);

    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    // handle the event for the gui
    g_gui->event_handler(event);

    // if the event is a quit event, return success
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }

    g_visualizer->event_handler(event);

    return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    // Skip rendering if window is minimized
    if (SDL_GetWindowFlags(g_window) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return SDL_APP_CONTINUE;
    }

    // do the cpu updates here, before we do anything on the gpu
    g_scrubber->cpu_update();
    g_visualizer->cpu_update();

    // acquire a command buffer, this is the main command buffer for the frame
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(g_gpu_device);

    // begin a copy pass, this is used to copy data from the cpu to the gpu
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    g_scrubber->copy_pass(g_upload_buffer, copy_pass);
    g_visualizer->copy_pass(g_upload_buffer, copy_pass);
    SDL_EndGPUCopyPass(copy_pass);

    // now that data is ready on the cpu and gpu, we can do our main compute tasks
    g_visualizer->compute_pass(command_buffer);

    // call all functions that may render to a texture, and not the window itself.
    g_visualizer->render_pass(command_buffer);

    SDL_GPUTexture *swapchain_texture;
    SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, g_window, &swapchain_texture, nullptr, nullptr);

    if (swapchain_texture != nullptr) // if this is nullptr, can't really render anything
    {
        // Calculate FPS like old NOVA source code
        float frame_render_time = static_cast<float>(SDL_GetTicks());
        float fps = 1.0f / ((frame_render_time - g_last_frame_render_time) / 1000.0f);
        g_last_frame_render_time = frame_render_time;
        // Send fps data so that GUI can display it
        g_gui->prepare_to_render(command_buffer, fps);

        // Setup and start a render pass
        SDL_GPUColorTargetInfo target_info = {};
        target_info.texture = swapchain_texture;
        target_info.clear_color = SDL_FColor{0.45f, 0.55f, 0.60f, 1.00f};
        target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
        target_info.mip_level = 0;
        target_info.layer_or_depth_plane = 0;
        target_info.cycle = true;
        SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);
        g_gui->render(command_buffer, render_pass);
        SDL_EndGPURenderPass(render_pass);
    }

    // render all of the other mini windows made by imgui
    g_gui->render_viewports();

    // Submit the command buffer
    SDL_SubmitGPUCommandBuffer(command_buffer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{

    // Ensure writer thread exits
    g_writer_running = false;
    g_writer_thread_obj->join();

    // Ensure data acquisition thread exits
    g_data_acquisition_running = false;
    g_data_acquisition_thread_obj->join();

    SDL_WaitForGPUIdle(g_gpu_device);

    delete g_visualizer;
    delete g_scrubber;
    delete g_gui;
    delete g_upload_buffer;

    SDL_ReleaseWindowFromGPUDevice(g_gpu_device, g_window);
    SDL_DestroyGPUDevice(g_gpu_device);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
}