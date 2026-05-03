#include "util/pch.hh"

// DO NOT MOVE THIS TO PCH.HH, IT WILL NOT COMPILE 😊
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#include "data/DataAcquisition.hh"
#include "data/DataSource.hh"
#include "render/DigitalCodedExposure.hh"
#include "render/GPUDevice.hh"
#include "render/RenderTarget.hh"
#include "ui/Windows.hh"
#include "render/Visualizer.hh"
#include "slam_manager.hh"
#include "ui/GUI.hh"
#include "ui/Scrubber.hh"
#include "util/ErrorQueue.hh"

struct Application
{
    // Graphics
    GPUDevice gpu_device;
    SDL_Window *window = nullptr;
    float last_frame_render_time = 0.0f;

    // Modules
    std::unique_ptr<ErrorQueue> error_queue;
    std::unique_ptr<DataAcquisition> data_acq;
    std::unique_ptr<Visualizer> visualizer;
    std::unique_ptr<DigitalCodedExposure> digital_coded_exposure;
    std::unique_ptr<GUI> gui;
        std::unique_ptr<SlamManager> slam;

    // Initializes graphics of entire application
    SDL_AppResult init() {
        if (!gpu_device.is_open()) {
            return SDL_APP_FAILURE;
        }

        window = gpu_device.create_window(1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (window == nullptr) {
            return SDL_APP_FAILURE;
        }

        // Initialize modules
        error_queue = std::make_unique<ErrorQueue>();
        data_acq = std::make_unique<DataAcquisition>(gpu_device);
        visualizer = std::make_unique<Visualizer>(gpu_device);
        digital_coded_exposure = std::make_unique<DigitalCodedExposure>(gpu_device);
        gui = std::make_unique<GUI>(*data_acq, *visualizer, *error_queue, window, gpu_device);
        app->slam = std::make_unique<SlamManager>();

        return SDL_APP_CONTINUE;
    }

    void update() {
        // Update all of the data sources
        data_acq->update();
        
        if(app->visualizer->set_slam_pc_changed(app->slam->were_pointclouds_updated()))
        {
            app->visualizer->set_slam_pointcloud(app->slam->get_viz_pointcloud());
            app->visualizer->set_slam_global_pointcloud(app->slam->get_viz_global_pointcloud());
        }
        if(app->visualizer->set_slam_path_changed(app->slam->was_path_updated()))
        {
            app->visualizer->set_slam_path(app->slam->get_viz_path());
        }

        // Render all data sources
        std::vector<std::shared_ptr<DataSource>> data_sources = data_acq->get_data_sources();
        for (const auto& data_source : data_sources)
        {
            digital_coded_exposure->render(data_source);
            visualizer->render(data_source);
        }
    }

    void render() {
        SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device.get_SDL_device());
        SDL_GPUTexture *swapchain_texture;
        SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, nullptr, nullptr);

        if (swapchain_texture != nullptr)
        {
            float now = static_cast<float>(SDL_GetTicks());
            float fps = 1.0f / ((now - last_frame_render_time) / 1000.0f);
            last_frame_render_time = now;

            gui->prepare_to_render(command_buffer, fps);
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

        gui->render_viewports();
        SDL_SubmitGPUCommandBuffer(command_buffer);
    }
};


//////////////////////////////////////
/** 
 * NOVA consists of a single instance of the Application struct passed between the different 
 * SDL methods by the 'appstate' pointer. These callback functions are handled by SDL and will be 
 * called at the appropriate times to manage the Application. These probably shouldn't be changed.
 */
//////////////////////////////////////

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{   
    auto *app = new Application();
    if (app->init() == SDL_APP_FAILURE) {
        return SDL_APP_FAILURE;
    }
    
    // Assign to appstate for other callbacks to use
    *appstate = app;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    auto *app = static_cast<Application *>(appstate);

    app->gui->event_handler(event);

    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;

    if ((event->type == SDL_EVENT_KEY_DOWN) && (event->key.key == SDLK_BACKSPACE))
    {
        if (app->slam->isRunning())
        {
            app->slam->stopSlam();
            app->visualizer->set_slam_pointcloud(nullptr);
            app->visualizer->set_slam_global_pointcloud(nullptr);
            app->visualizer->set_slam_path(nullptr);
        }
        else
        {
            SlamManager::StartSlamParameters params;
            params.left_camera_yaml_path = "C:/Users/jackm/Desktop/nova/src/SLAM/esvo2_core/calib/dsec/zurich_city_04_a/left.yaml";
            params.right_camera_yaml_path = "C:/Users/jackm/Desktop/nova/src/SLAM/esvo2_core/calib/dsec/zurich_city_04_a/right.yaml";
            params.Mapping_yaml_path = "C:/Users/jackm/Desktop/nova/src/SLAM/esvo2_core/cfg/mapping/mapping_dsec_AA.yaml";
            params.Tracking_yaml_path = "C:/Users/jackm/Desktop/nova/src/SLAM/esvo2_core/cfg/tracking/tracking_dsec_AA.yaml";
            params.IR_Left_yaml_path = "C:/Users/jackm/Desktop/nova/src/SLAM/image_representation/cfg/image_representation_fast.yaml";
            params.IR_Right_yaml_path = "C:/Users/jackm/Desktop/nova/src/SLAM/image_representation/cfg/image_representation_fast_r.yaml";

            std::vector<std::shared_ptr<DataSource>> sources = app->data_acq->get_data_sources();
            if (sources.size() < 2)
            {
                std::cerr << "Need at least 2 data sources (left + right) to start SLAM" << std::endl;
                return SDL_APP_CONTINUE;
            }
            params.left_scrubber = &sources.at(0)->scrubber;
            params.left_eventdata = &sources.at(0)->event_data;
            params.right_scrubber = &sources.at(1)->scrubber;
            params.right_eventdata = &sources.at(1)->event_data;

            app->slam->startSlam(params);
        }
    }

     if ((event->type == SDL_EVENT_KEY_DOWN) && (event->key.key == SDLK_SPACE))
    {
        app->visualizer->toggle_display_global_pointcloud();
    }

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

    app->update();
    app->render();

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    auto *app = static_cast<Application *>(appstate);
    
    delete app;
    SDL_Quit();
}
