#include "util/pch.hh"

// DO NOT MOVE THIS TO PCH.HH, IT WILL NOT COMPILE 😊
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#include "data/DataAcquisition.hh"
#include "render/DigitalCodedExposure.hh"
#include "render/GPUDevice.hh"
#include "render/RenderTarget.hh"
#include "render/SpinningCube.hh"
#include "render/UploadBuffer.hh"
#include "render/Visualizer.hh"
#include "ui/GUI.hh"
#include "ui/Scrubber.hh"
#include "util/ErrorQueue.hh"
#include "util/threads.hh"
// #include "SLAM/refactor_files/slam_manager.cpp"

struct Application
{
        // Graphics
        GPUDevice gpu_device_wrapper;
        SDL_Window *window = nullptr;
        float last_frame_render_time = 0.0f;

        // Modules
        std::unique_ptr<ErrorQueue> error_queue;
        std::unique_ptr<DataAcquisition> data_acq;
        std::unique_ptr<Visualizer> visualizer;
        std::unique_ptr<DigitalCodedExposure> digital_coded_exposure;
        std::unique_ptr<GUI> gui;
        // std::unique_ptr<SlamManager> slam;

        // Worker threads
        std::atomic<bool> data_acquisition_running = true;
        std::thread data_acquisition_thread;
};

static SDL_AppResult init_graphics(Application &app)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    app.window = SDL_CreateWindow("Nova", 1280, 720, window_flags);
    if (app.window == nullptr)
    {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app.gpu_device_wrapper = std::move(GPUDevice(app.window));
    if (app.gpu_device_wrapper.get_device() == nullptr)
    {
        SDL_Log("Couldn't create GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

static void render_gui(void *appstate) {
    auto *app = static_cast<Application *>(appstate);

    // Render GUI
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(app->gpu_device_wrapper.get_device());
    SDL_GPUTexture *swapchain_texture;
    SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, app->window, &swapchain_texture, nullptr, nullptr);

    if (swapchain_texture != nullptr)
    {
        float now = static_cast<float>(SDL_GetTicks());
        float fps = 1.0f / ((now - app->last_frame_render_time) / 1000.0f);
        app->last_frame_render_time = now;

        app->gui->prepare_to_render(command_buffer, fps);

        SDL_GPUColorTargetInfo target_info = {};
        target_info.texture = swapchain_texture;
        target_info.clear_color = SDL_FColor{0.45f, 0.55f, 0.60f, 1.00f};
        target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
        target_info.mip_level = 0;
        target_info.layer_or_depth_plane = 0;
        target_info.cycle = true;
        SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);
        app->gui->render(command_buffer, render_pass);
        SDL_EndGPURenderPass(render_pass);
    }

    app->gui->render_viewports();
    SDL_SubmitGPUCommandBuffer(command_buffer);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{  
    // Initialize application and save to appstate so that it can be accessed in other functions
    auto *app = new Application();
    *appstate = app;

    // Initialize graphics
    if (init_graphics(*app) == SDL_APP_FAILURE)
        return SDL_APP_FAILURE;

    // Initialize modules
    app->error_queue = std::make_unique<ErrorQueue>();
    app->data_acq = std::make_unique<DataAcquisition>(app->gpu_device_wrapper.get_device());
    app->visualizer = std::make_unique<Visualizer>(app->gpu_device_wrapper.get_device());
    app->digital_coded_exposure = std::make_unique<DigitalCodedExposure>(app->gpu_device_wrapper.get_device());
    app->gui = std::make_unique<GUI>(*app->data_acq, *app->visualizer, *app->error_queue, 
                                     app->window, app->gpu_device_wrapper.get_device());
    // app->slam = std::make_unique<SlamManager>();

    // Spawn separate thread to manage the DataAcquisition
    app->data_acquisition_thread = std::thread(program_thread::data_acquisition_thread,
                                                std::ref(app->data_acquisition_running),
                                                std::ref(*app->data_acq));

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    auto *app = static_cast<Application *>(appstate);

    app->gui->event_handler(event);

    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    auto *app = static_cast<Application *>(appstate);

    if (SDL_GetWindowFlags(app->window) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return SDL_APP_CONTINUE;
    }


    // Update all of the data sources
    app->data_acq->update();
    
    // Update SLAM based on its scrubbers's events
    // app->slam->send_events();
    
    // Render all data sources
    std::vector<std::shared_ptr<DataSource>> data_sources = app->data_acq->get_data_sources();
    for (const auto& data_source : data_sources)
    {
        app->digital_coded_exposure->render(data_source);
        app->visualizer->render(data_source);
    }

    // Render the GUI
    render_gui(appstate);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    auto *app = static_cast<Application *>(appstate);

    // Stop worker threads
    app->data_acquisition_running = false;
    app->data_acquisition_thread.join();

    // It's important to free window after we delete app b/c GPUDevice wrapper releases window from GPU
    SDL_Window *window = app->window;
    delete app;
    
    SDL_DestroyWindow(window);
    SDL_Quit();
}
