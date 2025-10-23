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

    public:
        GUI(std::unordered_map<std::string, RenderTarget> &render_targets, ParameterStore *parameter_store,
            SDL_Window *window, SDL_GPUDevice *gpu_device)
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

        void prepare_to_render(SDL_GPUCommandBuffer *command_buffer)
        {
            // Start the Dear ImGui frame
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            // Create a simple demo window
            ImGui::ShowDemoWindow();

            ImGui::Begin("Spinning Cube Viewport");

            // Check if the render target map and the specific target exist
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

                    // Check if the item (image) we just rendered is hovered
                    render_targets.at("SpinningCubeColor").is_focused = ImGui::IsItemHovered();
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