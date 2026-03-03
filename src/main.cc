#include "pch.hh"

#include "DataAcquisition.hh"
#include "DataWriter.hh"
#include "DigitalCodedExposure.hh"
#include "GUI.hh"
#include "ParameterStore.hh"
#include "RenderTarget.hh"
#include "Scrubber.hh"
#include "SpinningCube.hh"
#include "UploadBuffer.hh"
#include "Visualizer.hh"
#include "ErrorQueue.hh"
#include "threads.hh" 

#include <memory>




//! Current only supports a single GPU device
SDL_GPUDevice *gpu_device = nullptr;

//! SDL stuff
SDL_Window *window = nullptr;
std::unordered_map<std::string, RenderTarget> render_targets;
float last_frame_render_time = 0.0f; 

//! Modules (uses unique_ptr for RAII, prevents accidental copies and double deletes, and allows
//! for modules to hold references to one another)
std::unique_ptr<UploadBuffer>           upload_buffer;
std::unique_ptr<GUI>                    gui;
std::unique_ptr<Scrubber>               scrubber;
std::unique_ptr<Visualizer>             visualizer;
std::unique_ptr<DigitalCodedExposure>   digital_coded_exposure;
std::unique_ptr<EventData>              event_data;
std::unique_ptr<DataAcquisition>        data_acq;
std::unique_ptr<DataWriter>             data_writer;
std::unique_ptr<ErrorQueue>             error_queue;

//! Worker threads run until their respective boolean flag is set false (see SDL_Quit)
std::atomic<bool> writer_running = true;
std::thread writer_thread;

std::atomic<bool> data_acquisition_thread_running = true;
std::thread data_acquisition_thread;


/**
 * @brief Initializes the SDL window and GPU device then links them together. Called in SDL_AppInit()
 */
SDL_AppResult init_graphics() {
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create SDL window
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window = SDL_CreateWindow("Nova", 1280, 720, window_flags);
    if (window == nullptr)
    {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    // Create GPU Device
    gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");
    if (gpu_device == nullptr)
    {
        SDL_Log("Couldn't create GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Claim window for GPU Device
    if (!SDL_ClaimWindowForGPUDevice(gpu_device, window))
    {
        SDL_Log("Couldn't claim window for GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetGPUSwapchainParameters(gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);

    return SDL_APP_CONTINUE;
}


/**
 * @brief This function runs once at startup, initializing the entire program
 */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{

    // Initialize SDL window & gpu device
    if (init_graphics() == SDL_APP_FAILURE) return SDL_APP_FAILURE;


    // Initialize modules - done within a GPU copy pass to allow for modules to upload data
    // to the GPU during initialization (e.g. static meshes, grid lines, etc.)
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    
    // Modules passed other modules by reference
    error_queue             = std::make_unique<ErrorQueue>();
    upload_buffer           = std::make_unique<UploadBuffer>(gpu_device);
    scrubber                = std::make_unique<Scrubber>(*event_data, gpu_device, *error_queue);
    visualizer              = std::make_unique<Visualizer>(*event_data, *scrubber, *upload_buffer, render_targets, window, gpu_device, copy_pass, *error_queue);
    digital_coded_exposure  = std::make_unique<DigitalCodedExposure>(*event_data, *scrubber, render_targets, window, gpu_device, *error_queue);
    data_acq                = std::make_unique<DataAcquisition>(*error_queue);
    data_writer             = std::make_unique<DataWriter>(*error_queue);
    gui                     = std::make_unique<GUI>(render_targets, window, gpu_device, scrubber, *error_queue);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
    // -----


    // Initialize threads (references passed to std::thread decay to normal value unless you use std::ref)
    writer_thread = std::thread(program_thread::writer_thread, 
                                    std::ref(writer_running),
                                    std::ref(data_writer));

    data_acquisition_thread = std::thread(program_thread::data_acquisition_thread, 
                                                std::ref(data_acquisition_thread_running), 
                                                std::ref(data_acq),
                                                std::ref(event_data), 
                                                std::ref(data_writer),
                                                std::ref(digital_coded_exposure));
    // -----
    
    return SDL_APP_CONTINUE;
}

/**
 * @brief This function runs when a new event (mouse input, keypresses, etc) occurs.
 */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    // handle the event for the gui
    gui->event_handler(event);

    // if the event is a quit event, return success
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }

    visualizer->event_handler(event);

    return SDL_APP_CONTINUE;
}



/**
 * @brief This function runs once per frame, and is the heart of the program.
 */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    // Skip rendering if window is minimized
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return SDL_APP_CONTINUE;
    }

    // do the cpu updates here, before we do anything on the gpu
    scrubber->cpu_update();
    visualizer->cpu_update();
    digital_coded_exposure->cpu_update();

    // acquire a command buffer, this is the main command buffer for the frame
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);

    // begin a copy pass, this is used to copy data from the cpu to the gpu
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    scrubber->copy_pass(*upload_buffer, copy_pass);
    visualizer->copy_pass(*upload_buffer, copy_pass);
    digital_coded_exposure->copy_pass(*upload_buffer, copy_pass);
    SDL_EndGPUCopyPass(copy_pass);

    // now that data is ready on the cpu and gpu, we can do our main compute tasks
    visualizer->compute_pass(command_buffer);
    digital_coded_exposure->compute_pass(command_buffer);

    // call all functions that may render to a texture, and not the window itself.
    visualizer->render_pass(command_buffer);
    digital_coded_exposure->render_pass(command_buffer);

    SDL_GPUTexture *swapchain_texture;
    SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, nullptr, nullptr);

    if (swapchain_texture != nullptr) // if this is nullptr, can't really render anything
    {
        // Calculate FPS like old NOVA source code
        float frame_render_time = static_cast<float>(SDL_GetTicks());
        float fps = 1.0f / ((frame_render_time - last_frame_render_time) / 1000.0f);
        last_frame_render_time = frame_render_time;

        // Send fps data so that GUI can display it
        gui->prepare_to_render(command_buffer, fps);

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
        gui->render(command_buffer, render_pass);
        SDL_EndGPURenderPass(render_pass);
    }

    // render all of the other mini windows made by imgui
    gui->render_viewports();

    // Submit the command buffer
    SDL_SubmitGPUCommandBuffer(command_buffer);

    return SDL_APP_CONTINUE;
}



/**
 * @brief On quit. Cleans up stuff.
 */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{

    // Ensure writer thread exits
    writer_running = false;
    writer_thread.join();

    // Ensure data acquisition thread exits
    data_acquisition_thread_running = false;
    data_acquisition_thread.join();

    // Flush file write buffer?
    data_writer->clear();

    // Ensure camera disconnect
    data_acq->clear();

    SDL_WaitForGPUIdle(gpu_device);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();
}