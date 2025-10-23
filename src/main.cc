#include "pch.hh"

#include "GUI.hh"
#include "ParameterStore.hh"
#include "RenderTarget.hh"
#include "UploadBuffer.hh"
#include "SpinningCube.hh"

ParameterStore *g_parameter_store = nullptr;

SDL_Window *g_window = nullptr;
SDL_GPUDevice *g_gpu_device = nullptr;

UploadBuffer *g_upload_buffer = nullptr;
GUI *g_gui = nullptr;
SpinningCube *g_spinning_cube = nullptr;

std::unordered_map<std::string, RenderTarget> g_render_targets;

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
    SDL_SetGPUSwapchainParameters(g_gpu_device, g_window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                  SDL_GPU_PRESENTMODE_VSYNC);

    g_upload_buffer = new UploadBuffer(g_gpu_device);

    // if we have a single use upload, like uploading a static mesh, might as well do it here
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(g_gpu_device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);

    g_gui = new GUI(g_render_targets, g_parameter_store, g_window, g_gpu_device);

    g_spinning_cube = new SpinningCube(g_gpu_device, g_upload_buffer, copy_pass, g_render_targets);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    // app quit takes precedence over everything else
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }

    // if the spinning cube handled the event, return continue
    if (g_spinning_cube->event_handler(event))
    {
        return SDL_APP_CONTINUE;
    }

    // if the gui wants to capture the event, handle it
    if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
    {
        g_gui->event_handler(event);
        return SDL_APP_CONTINUE;
    }

    // if no one handled the event, return continue
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

    // data aq hopefully is being done on another thread, if not do it here
    g_spinning_cube->update();

    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(g_gpu_device);

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    // event data needs to sync with the gpu, all points + images needs to be ready to go
    // event_data.sync_to_gpu(...)
    SDL_EndGPUCopyPass(copy_pass);

    // now that data is ready on the cpu and gpu, we can do our main compute tasks
    // 3dvisualizer->update(...) either cpu or gpu depending on how y'all structure this
    // digital_shutter->update(...) the data is already ready from the event_data sync, so compute shader stuff now

    // call all functions that may render to a texture, and not the window itself.
    g_spinning_cube->render(command_buffer);


    SDL_GPUTexture *swapchain_texture;
    SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, g_window, &swapchain_texture, nullptr, nullptr);

    if (swapchain_texture != nullptr) // if this is nullptr, can't really render anything
    {
        g_gui->prepare_to_render(command_buffer);

        // Setup and start a render pass
        SDL_GPUColorTargetInfo target_info = {};
        target_info.texture = swapchain_texture;
        target_info.clear_color = SDL_FColor{0.45f, 0.55f, 0.60f, 1.00f};
        target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
        target_info.mip_level = 0;
        target_info.layer_or_depth_plane = 0;
        target_info.cycle = false;
        SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);


        g_gui->render(command_buffer, render_pass);
        SDL_EndGPURenderPass(render_pass);
    }

    // Submit the command buffer
    SDL_SubmitGPUCommandBuffer(command_buffer);

    // render all of the other mini windows made by imgui
    g_gui->render_viewports();

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    delete g_spinning_cube;
    delete g_gui;
    delete g_upload_buffer;

    SDL_WaitForGPUIdle(g_gpu_device);
    SDL_ReleaseWindowFromGPUDevice(g_gpu_device, g_window);
    SDL_DestroyGPUDevice(g_gpu_device);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
}