#pragma once

#ifndef GUI_HH
#define GUI_HH

#include "pch.hh"

#include "ParameterStore.hh"
#include "RenderTarget.hh"

#include "fonts/CascadiaCode.ttf.h"

class GUI
{
    private:
        std::unordered_map<std::string, RenderTarget> &render_targets;
        ParameterStore *parameter_store;
        SDL_Window *window = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;
        ImDrawData *draw_data = nullptr;

        static inline const std::string time_units[] = {"(s)", "(ms)", "(us)"};

    public:

        enum class TIME
        {
            UNIT_S = 0,
            UNIT_MS = 1,
            UNIT_US = 2
        };

        enum class SHUTTER
        {
            TIME_BASED = 0,
            EVENT_BASED = 1
        };

        GUI(std::unordered_map<std::string, RenderTarget> &render_targets, ParameterStore *parameter_store, SDL_Window *window, SDL_GPUDevice *gpu_device)
            : render_targets(render_targets), parameter_store(parameter_store), window(window), gpu_device(gpu_device)
        {
            // Setup Dear ImGui context
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
            void *font_memory = malloc(sizeof CascadiaCode_ttf);
            std::memcpy(font_memory, CascadiaCode_ttf, sizeof CascadiaCode_ttf);
            io.Fonts->AddFontFromMemoryTTF(font_memory, sizeof CascadiaCode_ttf, 16.0f);

            // Setup Dear ImGui style
            ImGui::StyleColorsDark();

            // Setup scaling
            float scaling_factor = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
            ImGuiStyle &style = ImGui::GetStyle();
            style.ScaleAllSizes(scaling_factor);
            style.FontScaleDpi = scaling_factor;
            io.ConfigDpiScaleFonts = true;
            io.ConfigDpiScaleViewports = true;

            // Setup Platform/Renderer backends
            ImGui_ImplSDL3_InitForSDLGPU(window);
            ImGui_ImplSDLGPU3_InitInfo init_info = {.Device = gpu_device,
                                                    .ColorTargetFormat =
                                                        SDL_GetGPUSwapchainTextureFormat(gpu_device, window),
                                                    .MSAASamples = SDL_GPU_SAMPLECOUNT_1,
                                                    .SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                                    .PresentMode = SDL_GPU_PRESENTMODE_VSYNC};
            ImGui_ImplSDLGPU3_Init(&init_info);
        }

        ~GUI()
        {
            // Cleanup ImGui
            ImGui_ImplSDL3_Shutdown();
            ImGui_ImplSDLGPU3_Shutdown();
            ImGui::DestroyContext();
        }

        void event_handler(SDL_Event *event)
        {
            ImGui_ImplSDL3_ProcessEvent(event);
        }
        
        // https://github.com/ocornut/imgui/wiki/Docking
        // Creates required dockspace before rendering ImGui windows on top
        // From old NOVA source code
        void draw_GUI_dockspace() {
            static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
            ImGuiWindowFlags w_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;


            ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);

            w_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
            

            ImGui::Begin("DockSpace", nullptr, w_flags);
            ImGuiIO& io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
                ImGuiID dockspace_id = ImGui::GetID("DockSpace");
                ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
            }
            ImGui::End();
        }


        // Recreate info block from old NOVA
        void draw_info_block()
        {
            ImGui::Begin("Info");
            if(parameter_store -> exists("camera_pos"))
            {
                const glm::vec3 &camera_pos{parameter_store -> get<glm::vec3>("camera_pos")};
                ImGui::Text("Camera (World): (%.3f, %.3f, %.3f)", camera_pos.x, camera_pos.y, camera_pos.z);
            }
            else
            {
                ImGui::Text("No Camera (World)");
            }
            
            ImGui::Separator();
            
            if(!parameter_store -> exists("particle_scale"))
            {
                parameter_store -> add("particle_scale", 3.0f); // Default particle scale
            }

            float particle_scale{parameter_store -> get<float>("particle_scale")}; // Not reference because changing object values via pointer seems like bad practice?
            ImGui::SliderFloat("Particle Scale", &particle_scale, 0.1f, 6.0f);
            parameter_store -> add("particle_scale", particle_scale);

            ImGui::Separator();

            
            if(!parameter_store -> exists("polarity_neg_color"))
            {
                parameter_store -> add("polarity_neg_color", glm::vec3(1.0f, 0.0f, 0.0f)); // Default particle scale
            }

            glm::vec3 polarity_neg_color{parameter_store -> get<glm::vec3>("polarity_neg_color")};

            if(!parameter_store -> exists("polarity_pos_color"))
            {
                parameter_store -> add("polarity_pos_color", glm::vec3(0.0f, 1.0f, 0.0f)); // Default particle scale
            }
            glm::vec3 polarity_pos_color{parameter_store -> get<glm::vec3>("polarity_pos_color")};

            ImGui::ColorEdit3("Negative Polarity Color", (float*) &polarity_neg_color);
            ImGui::ColorEdit3("Positive Polarity Color", (float*) &polarity_pos_color);

            parameter_store -> add("polarity_neg_color", polarity_neg_color);
            parameter_store -> add("polarity_pos_color", polarity_pos_color);

            ImGui::Separator();

            if(!parameter_store -> exists("event_contrib_weight"))
            {
                parameter_store -> add("event_contrib_weight", 0.5f);
            }  
            float event_contrib_weight{parameter_store -> get<float>("event_contrib_weight")};
            ImGui::SliderFloat("Event Contribution Weight", &event_contrib_weight, 0.0f, 1.0f);
            parameter_store -> add("event_contrib_weight", event_contrib_weight);

            ImGui::Separator();

            if(!parameter_store -> exists("unit_type"))
            {
                parameter_store -> add("unit_type", 1);
            }
            int32_t unit_type{parameter_store -> get<int32_t>("unit_type")};
            
            const int32_t units[] = {1000000000, 1000, 1};

            ImGui::Combo("Time Unit", &unit_type, "s\0ms\0us\0"); 
            parameter_store -> add("unit_time_conversion_factor", units[unit_type]);
            parameter_store -> add("unit_type", unit_type);
            
            ImGui::Separator();

            

            ImGui::Text("Processing options"); 

            if(!parameter_store -> exists("shutter_type"))
            {
                parameter_store -> add("shutter_type", 0);
            }
            int32_t shutter_type{parameter_store -> get<int32_t>("shutter_type")};
            ImGui::Combo("Shutter", &shutter_type, "Time Based\0Event Based\0");
            parameter_store -> add("shutter_type", shutter_type);

            if (static_cast<GUI::SHUTTER>(parameter_store -> get<int32_t>("shutter_type")) == GUI::SHUTTER::TIME_BASED) {
                
                if(parameter_store -> exists("time_window_begin") && parameter_store -> exists("time_window_end"))
                {
                    float time_window_end{parameter_store->get<float>("time_window_end")};
                    float time_window_begin{parameter_store->get<float>("time_window_begin")};
                    float frameLength_T = time_window_end - time_window_begin;
                    
                    // Get user input for beginning of time shutter window
                    if(!parameter_store->exists("time_shutter_window_begin"))
                    {
                        parameter_store->add("time_shutter_window_begin", time_window_begin);
                    }
                    float time_shutter_window_begin{parameter_store->get<float>("time_shutter_window_begin")};
                    std::string shutter_initial{"Shutter Initial "};
                    shutter_initial.append(time_units[parameter_store->get<int32_t>("unit_type")]);
                    ImGui::SliderFloat(shutter_initial.c_str(), &time_shutter_window_begin, 0, frameLength_T, "%.4f"); 

                    // Get user input for ending of time shutter window
                    if(!parameter_store->exists("time_shutter_window_end"))
                    {
                        parameter_store->add("time_shutter_window_end", time_window_end);
                    }
                    float time_shutter_window_end{parameter_store->get<float>("time_shutter_window_end")};
                    std::string shutter_final{"Shutter Final "};
                    shutter_final.append(time_units[parameter_store->get<int32_t>("unit_type")]);
                    ImGui::SliderFloat(shutter_final.c_str(), &time_shutter_window_end, 0, frameLength_T, "%.4f");  

                    // Not sure if clamping is necessary
                    // evtData->getTimeShutterWindow_L() = std::clamp(evtData->getTimeShutterWindow_L(), 0.0f, frameLength_T);
                    // evtData->getTimeShutterWindow_R() = std::clamp(evtData->getTimeShutterWindow_R(), evtData->getTimeShutterWindow_L(), frameLength_T);
                    
                    // Add user time shutter window input to parameter store
                    parameter_store -> add("time_shutter_window_begin", time_shutter_window_begin);
                    parameter_store -> add("time_shutter_window_end", time_shutter_window_end);
                }
            }
            else if (static_cast<GUI::SHUTTER>(parameter_store -> get<int32_t>("shutter_type")) == GUI::SHUTTER::EVENT_BASED) {
            
                if(parameter_store -> exists("event_window_begin") && parameter_store-> exists("event_window_end"))
                {
                    uint32_t event_window_end{parameter_store->get<uint32_t>("event_window_end")};
                    uint32_t event_window_begin{parameter_store->get<uint32_t>("event_window_begin")};
                    uint32_t frameLength_E = event_window_end - event_window_begin;

                    if(!parameter_store->exists("event_shutter_window_begin"))
                    {
                        parameter_store->add("event_shutter_window_begin", event_window_begin);
                    }
                    uint32_t event_shutter_window_begin{parameter_store->get<uint32_t>("event_shutter_window_begin")};
                    ImGui::SliderInt("Shutter Initial (events)", (int *) &event_shutter_window_begin, 0, frameLength_E); 

                    if(!parameter_store->exists("event_shutter_window_end"))
                    {
                        parameter_store->add("event_shutter_window_end", event_window_end);
                    }
                    uint32_t event_shutter_window_end{parameter_store->get<uint32_t>("event_shutter_window_end")};
                    ImGui::SliderInt("Shutter Final (events)", (int *) &event_shutter_window_end, 0, frameLength_E);   
                    
                    // Not sure if clamping is necessary
                    // evtData->getEventShutterWindow_L() = std::clamp(evtData->getEventShutterWindow_L(), (uint) 0, frameLength_E);
                    // evtData->getEventShutterWindow_R() = std::clamp(evtData->getEventShutterWindow_R(), evtData->getEventShutterWindow_L(), frameLength_E); 
                    
                    // Add user input back to parameter store
                    parameter_store -> add("event_shutter_window_begin", event_shutter_window_begin);
                    parameter_store -> add("event_shutter_window_end", event_shutter_window_end);
                }      
            }
        
            // Auto update controls
            if(!parameter_store -> exists("shutter_fps"))
            {
                parameter_store -> add("shutter_fps", 0.0f);
            }
            float shutter_fps{parameter_store -> get<float>("shutter_fps")};
            ImGui::SliderFloat("FPS", &shutter_fps, 0.0f, 100.0f);
            parameter_store -> add("shutter_fps", shutter_fps);

            // TODO Implement buttons
            if (ImGui::Button("Play (Time period)")) {
            
            }
            if (ImGui::Button("Play (Events period)")) {

            }

            // "Post" processing
            if(!parameter_store -> exists("shutter_frequency"))
            {
                parameter_store -> add("shutter_frequency", 100.0f);
            }
            float shutter_frequency{parameter_store -> get<float>("shutter_frequency")};
            ImGui::SliderFloat("Frequency (Hz)", &shutter_frequency, 0.001f, 250.0f);
            parameter_store -> add("shutter_frequency", shutter_frequency);

            if(parameter_store->exists("time_window_begin") && parameter_store->exists("time_window_end"))
            {
                if(!parameter_store -> exists("fwhm"))
                {
                    parameter_store -> add("fwhm", 0.0014f);
                }
                float fwhm{parameter_store->get<float>("fwhm")};
                std::string fwhm_str{"Full Width at Half Measure "};
                fwhm_str.append(time_units[parameter_store->get<int32_t>("unit_type")]);
            
                ImGui::SliderFloat(fwhm_str.c_str(), &fwhm, 0.0001f, (parameter_store->get<float>("time_window_after") - parameter_store->get<float>("time_window_after")) * 0.5, "%.4f");
            }

            if(!parameter_store->exists("shutter_is_morlet"))
            {
                parameter_store->add("shutter_is_morlet", false);
            }
            bool shutter_is_morlet{parameter_store->get<bool>("shutter_is_morlet")};
            ImGui::Checkbox("Morlet Shutter", &shutter_is_morlet);
            parameter_store->add("shutter_is_morlet", shutter_is_morlet);

            if(!parameter_store->exists("shutter_is_pca"))
            {
                parameter_store->add("shutter_is_pca", false);
            }
            bool shutter_is_pca{parameter_store->get<bool>("shutter_is_pca")};
            ImGui::Checkbox("PCA", &shutter_is_pca);
            parameter_store->add("shutter_is_pca", shutter_is_pca);

            if(!parameter_store->exists("shutter_is_positive_only"))
            {
                parameter_store->add("shutter_is_positive_only", false);
            }
            bool shutter_is_positive_only{parameter_store->get<bool>("shutter_is_positive_only")};
            ImGui::Checkbox("Positive Events Only", &shutter_is_positive_only);
            parameter_store->add("shutter_is_positive_only", shutter_is_positive_only);
            //ImGui::Separator();

            // TODO implement video recording
            // Video (ffmpeg) controls
            // ImGui::Text("Video options"); // TODO add documentation
            // inputTextWrapper(video_name); // TODO consider including library to allow inputting string as parameter
            // if (ImGui::Button("Start Record")) {
            //     recording = true;
            // }
            // if (ImGui::Button("Stop Record")) {
            //     recording = false;
            // }

            ImGui::End();
        }

        // This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index buffer!
        void prepare_to_render(SDL_GPUCommandBuffer *command_buffer)
        {
            // Start the Dear ImGui frame
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            // Draw the dockspace
            draw_GUI_dockspace();

            // Draw info block
            draw_info_block();
            // Create a simple demo window
            ImGui::ShowDemoWindow();
            
            ImGui::Begin("Spinning Cube Viewport");

            //// Check if the render target map and the specific target exist
            if (render_targets.count("SpinningCubeColor"))
            {
               SDL_GPUTexture *texture = render_targets.at("SpinningCubeColor").texture;
               if (texture)
               {
                   // Get the available pane size
                   ImVec2 pane_size = ImGui::GetContentRegionAvail();

                   // Get texture dimensions to calculate aspect ratio
                   Uint32 tex_w, tex_h;
                   float tex_aspect = (float)render_targets.at("SpinningCubeColor").width /
                                      (float)render_targets.at("SpinningCubeColor").height;

                   // Calculate display size to fit the pane while maintaining aspect ratio
                   ImVec2 display_size = pane_size;
                   float pane_aspect = pane_size.x / pane_size.y;

                   if (tex_aspect > pane_aspect)
                   {
                       // Texture is wider than pane, fit to width
                       display_size.y = pane_size.x / tex_aspect;
                   }
                   else
                   {
                       // Texture is taller than pane (or same aspect), fit to height
                       display_size.x = pane_size.y * tex_aspect;
                   }

                   // Center the image within the pane
                   float x_pad = (pane_size.x - display_size.x) * 0.5f;
                   float y_pad = (pane_size.y - display_size.y) * 0.5f;
                   ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x_pad);
                   ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_pad);

                   // Display the image. ImTextureID is typedef'd to SDL_GPUTexture*
                   ImGui::Image((ImTextureID)texture, display_size);
               }
               else
               {
                   ImGui::Text("Texture for 'SpinningCubeColor' is null.");
               }
            }
            else
            {
               ImGui::Text("Render target 'SpinningCubeColor' not found.");
            }
            ImGui::End();

            // Rendering
            ImGui::Render();
            draw_data = ImGui::GetDrawData();

            ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);
        }

        void render(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass)
        {
            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);
        }

        void render_viewports()
        {
            // Update and Render additional Platform Windows
            ImGuiIO &io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
        }
};

#endif // GUI_HH