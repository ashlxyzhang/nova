#include "util/pch.hh"

#include "data/DataAcquisition.hh"
#include "data/DataWriter.hh"
#include "render/DigitalCodedExposure.hh"
#include "render/RenderTarget.hh"
#include "render/SpinningCube.hh"
#include "render/UploadBuffer.hh"
#include "render/Visualizer.hh"
#include "ui/GUI.hh"
#include "ui/Scrubber.hh"
#include "util/ErrorQueue.hh"
#include "util/threads.hh"

struct Application
{
        // Graphics
        SDL_GPUDevice *gpu_device = nullptr;
        SDL_Window *window = nullptr;
        std::unordered_map<std::string, RenderTarget> render_targets;
        float last_frame_render_time = 0.0f;

        // Modules
        std::unique_ptr<ErrorQueue> error_queue;
        std::unique_ptr<EventData> event_data;
        std::unique_ptr<UploadBuffer> upload_buffer;
        std::unique_ptr<Scrubber> scrubber;
        std::unique_ptr<Visualizer> visualizer;
        std::unique_ptr<DigitalCodedExposure> digital_coded_exposure;
        std::unique_ptr<DataAcquisition> data_acq;
        std::unique_ptr<DataWriter> data_writer;
        std::unique_ptr<GUI> gui;

        // Worker threads
        std::atomic<bool> writer_running = true;
        std::thread writer_thread;
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

    app.gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");
    if (app.gpu_device == nullptr)
    {
        SDL_Log("Couldn't create GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(app.gpu_device, app.window))
    {
        SDL_Log("Couldn't claim window for GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetGPUSwapchainParameters(app.gpu_device, app.window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                  SDL_GPU_PRESENTMODE_VSYNC);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    auto *app = new Application();
    *appstate = app;

    if (init_graphics(*app) == SDL_APP_FAILURE)
        return SDL_APP_FAILURE;

    // Initialize modules within a GPU copy pass so they can upload data during init
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(app->gpu_device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);

    app->error_queue = std::make_unique<ErrorQueue>();
    app->event_data = std::make_unique<EventData>();
    app->upload_buffer = std::make_unique<UploadBuffer>(app->gpu_device);
    app->scrubber = std::make_unique<Scrubber>(*app->event_data, app->gpu_device, *app->error_queue);
    app->visualizer =
        std::make_unique<Visualizer>(*app->event_data, *app->scrubber, *app->upload_buffer, app->render_targets,
                                     app->window, app->gpu_device, copy_pass, *app->error_queue);
    app->digital_coded_exposure = std::make_unique<DigitalCodedExposure>(
        *app->event_data, *app->scrubber, app->render_targets, app->window, app->gpu_device, *app->error_queue);
    app->data_acq = std::make_unique<DataAcquisition>(*app->error_queue);
    app->data_writer = std::make_unique<DataWriter>(*app->error_queue);
    app->gui =
        std::make_unique<GUI>(app->render_targets, *app->data_acq, *app->data_writer, *app->scrubber, *app->visualizer,
                              *app->digital_coded_exposure, *app->error_queue, app->window, app->gpu_device);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);

    // Start worker threads
    app->writer_thread =
        std::thread(program_thread::writer_thread, std::ref(app->writer_running), std::ref(*app->data_writer));
    app->data_acquisition_thread =
        std::thread(program_thread::data_acquisition_thread, std::ref(app->data_acquisition_running),
                    std::ref(*app->data_acq), std::ref(*app->event_data), std::ref(*app->data_writer),
                    std::ref(*app->digital_coded_exposure), std::ref(*app->scrubber));

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    auto *app = static_cast<Application *>(appstate);

    app->gui->event_handler(event);

    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;

    app->visualizer->event_handler(event);

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

    // CPU updates
    app->scrubber->cpu_update();
    app->visualizer->cpu_update();
    app->digital_coded_exposure->cpu_update();

    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(app->gpu_device);

    // Copy pass: upload CPU data to GPU
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    app->scrubber->copy_pass(*app->upload_buffer, copy_pass);
    app->visualizer->copy_pass(*app->upload_buffer, copy_pass);
    app->digital_coded_exposure->copy_pass(*app->upload_buffer, copy_pass);
    SDL_EndGPUCopyPass(copy_pass);

    // Compute passes
    app->visualizer->compute_pass(command_buffer);
    app->digital_coded_exposure->compute_pass(command_buffer);

    // Off-screen render passes
    app->visualizer->render_pass(command_buffer);
    app->digital_coded_exposure->render_pass(command_buffer);

    // Swapchain render pass
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

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    auto *app = static_cast<Application *>(appstate);

    // Stop worker threads
    app->writer_running = false;
    app->writer_thread.join();
    app->data_acquisition_running = false;
    app->data_acquisition_thread.join();

    // Flush and disconnect
    app->data_writer->clear();
    app->data_acq->clear();

    // Free modules before GPU shutdown
    app->gui.reset();
    app->upload_buffer.reset();
    app->scrubber.reset();
    app->visualizer.reset();
    app->digital_coded_exposure.reset();
    app->event_data.reset();
    app->data_acq.reset();
    app->data_writer.reset();
    app->error_queue.reset();

    SDL_WaitForGPUIdle(app->gpu_device);
    SDL_ReleaseWindowFromGPUDevice(app->gpu_device, app->window);
    SDL_DestroyGPUDevice(app->gpu_device);
    SDL_DestroyWindow(app->window);

    delete app;
    SDL_Quit();
}
