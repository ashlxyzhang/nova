#pragma once
#ifndef DIGITAL_CODED_EXPOSURE_HH
#define DIGITAL_CODED_EXPOSURE_HH

#include "pch.hh"

#include "Camera.hh"
#include "EventData.hh"
#include "ParameterStore.hh"
#include "RenderTarget.hh"
#include "UploadBuffer.hh"

class DigitalCodedExposure
{
    public:
        DigitalCodedExposure(ParameterStore *parameter_store,
                             std::unordered_map<std::string, RenderTarget> &render_targets, EventData &event_data,
                             SDL_Window *window, SDL_GPUDevice *gpu_device, UploadBuffer *upload_buffer,
                             SDL_GPUCopyPass *copy_pass)
            : parameter_store(parameter_store), render_targets(render_targets), event_data(event_data), window(window),
              gpu_device(gpu_device) {
                // create the color texture, this is the texture that will store the color data
                SDL_GPUTextureCreateInfo color_create_info = {
                    .type = SDL_GPU_TEXTURETYPE_2D,
                    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
                    .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
                    .width = 1920,
                    .height = 1080,
                    .layer_count_or_depth = 1,
                    .num_levels = 1,
                    .sample_count = SDL_GPU_SAMPLECOUNT_1,
                };
                render_targets["DigitalCodedExposure"] = {SDL_CreateGPUTexture(gpu_device, &color_create_info), 1920, 1080};

              }
              
        ~DigitalCodedExposure()
        {
        }

        bool event_handler(SDL_Event *event)
        {
        }
        void cpu_update()
        {
        }
        void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        {
        }
        void compute_pass(SDL_GPUCommandBuffer *command_buffer)
        {
        }
        void render_pass(SDL_GPUCommandBuffer *command_buffer)
        {
            SDL_GPUColorTargetInfo color_target_info = {0};
            color_target_info.texture = render_targets["DigitalCodedExposure"].texture;
            SDL_FColor clear_col = {1.0f, 0.0f, 0.0f, 1.0f};
            color_target_info.clear_color = clear_col;
            color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
            color_target_info.store_op = SDL_GPU_STOREOP_STORE;
            color_target_info.mip_level = 0;
            color_target_info.layer_or_depth_plane = 0;
            color_target_info.cycle = false;

            SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, NULL);
            // SDL_BindGPUGraphicsPipeline(render_pass, pipeline);
            // SDL_GPUBufferBinding vertex_buffer_bindings[] = {{vertex_buffer, 0}};
            // SDL_BindGPUVertexBuffers(render_pass, 0, vertex_buffer_bindings, 1);
            // SDL_PushGPUVertexUniformData(command_buffer, 0, &mvp[0][0], sizeof(mvp)); // for testing only red square
            // SDL_DrawGPUPrimitives(render_pass, 36, 1, 0, 0);
            SDL_EndGPURenderPass(render_pass);
        }

    private:
        Camera camera;
        glm::vec3 box_min;
        glm::vec3 box_max;

        ParameterStore* parameter_store;
        std::unordered_map<std::string, RenderTarget> &render_targets;
        EventData &event_data;
        
        SDL_Window *window = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;

        SDL_GPUGraphicsPipeline *grid_pipeline = nullptr;
        SDL_GPUGraphicsPipeline *points_pipeline = nullptr;
        SDL_GPUGraphicsPipeline *frame_pipeline = nullptr;
        SDL_GPUGraphicsPipeline *text_pipeline = nullptr;
};

#endif