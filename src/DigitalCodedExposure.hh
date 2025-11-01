#pragma once
#ifndef DIGITAL_CODED_EXPOSURE_HH
#define DIGITAL_CODED_EXPOSURE_HH

#include "pch.hh"

#include "Camera.hh"
#include "EventData.hh"
#include "ParameterStore.hh"
#include "RenderTarget.hh"
#include "Scrubber.hh"
#include "UploadBuffer.hh"
#include "shaders/digital_coded_exposure/dce_comp.h"

#include <iostream>

struct PassData
{
        glm::vec4 col;
        glm::vec4 posOnly;
};

class DigitalCodedExposure
{
    private:
        ParameterStore *parameter_store;
        std::unordered_map<std::string, RenderTarget> &render_targets;
        EventData &event_data;
        Scrubber *scrubber = nullptr;

        SDL_Window *window = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;

        SDL_GPUComputePipeline *compute_pipeline = nullptr;

        std::string last_file = "";
        unsigned int width;
        unsigned int height;

    public:
        DigitalCodedExposure(ParameterStore *parameter_store,
                             std::unordered_map<std::string, RenderTarget> &render_targets, EventData &event_data,
                             SDL_Window *window, SDL_GPUDevice *gpu_device, UploadBuffer *upload_buffer,
                             Scrubber *scrubber, SDL_GPUCopyPass *copy_pass)
            : parameter_store(parameter_store), render_targets(render_targets), event_data(event_data), window(window),
              gpu_device(gpu_device), scrubber(scrubber)
        {
            // create the color texture, this is the texture that will store the color data
            SDL_GPUTextureCreateInfo color_create_info = {
                .type = SDL_GPU_TEXTURETYPE_2D,
                .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
                .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE,
                .width = 1920,
                .height = 1080,
                .layer_count_or_depth = 1,
                .num_levels = 1,
                .sample_count = SDL_GPU_SAMPLECOUNT_1,
            };
            render_targets["DigitalCodedExposure"] = {SDL_CreateGPUTexture(gpu_device, &color_create_info), 1920, 1080};

            SDL_GPUComputePipelineCreateInfo compute_pipeline_info = {0};
            compute_pipeline_info.code_size = sizeof(dce_comp);
            compute_pipeline_info.code = (Uint8 *)dce_comp;
            compute_pipeline_info.entrypoint = "main";
            compute_pipeline_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            compute_pipeline_info.num_samplers = 0;
            compute_pipeline_info.num_readonly_storage_textures = 0;
            compute_pipeline_info.num_readonly_storage_buffers = 1;
            compute_pipeline_info.num_readwrite_storage_textures = 1;
            compute_pipeline_info.num_readwrite_storage_buffers = 0;
            compute_pipeline_info.num_uniform_buffers = 1;
            compute_pipeline_info.threadcount_x = 1;
            compute_pipeline_info.threadcount_y = 1;
            compute_pipeline_info.threadcount_z = 1;

            compute_pipeline = SDL_CreateGPUComputePipeline(gpu_device, &compute_pipeline_info);
        }

        ~DigitalCodedExposure()
        {
            SDL_ReleaseGPUComputePipeline(gpu_device, compute_pipeline);
            SDL_ReleaseGPUTexture(gpu_device, render_targets["DigitalCodedExposure"].texture);
        }

        bool event_handler(SDL_Event *event)
        {
            return false;
        }
        void cpu_update()
        {
            event_data.lock_data_vectors();
            if (event_data.get_evt_vector_ref().empty())
            {
                event_data.unlock_data_vectors();
                return;
            }
            event_data.unlock_data_vectors();
            bool file_changed = false;

            file_changed = parameter_store->exists("streaming") && parameter_store->exists("stream_file_changed") &&
                           (parameter_store->get<bool>("streaming") &&
                            (last_file != parameter_store->get<std::string>("stream_file_name")));

            if (parameter_store->exists("load_file_name") &&
                last_file != parameter_store->get<std::string>("load_file_name"))
            {
                file_changed = true;
            }

            if (file_changed)
            {

                width = event_data.get_camera_event_resolution().x;
                height = event_data.get_camera_event_resolution().y;

                if (width == 0 || height == 0)
                {
                    return;
                }

                SDL_GPUTextureCreateInfo color_create_info = {
                    .type = SDL_GPU_TEXTURETYPE_2D,
                    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE,
                    .width = width,
                    .height = height,
                    .layer_count_or_depth = 1,
                    .num_levels = 1,
                    .sample_count = SDL_GPU_SAMPLECOUNT_1,
                };
                SDL_ReleaseGPUTexture(gpu_device, render_targets["DigitalCodedExposure"].texture);

                render_targets["DigitalCodedExposure"] = {SDL_CreateGPUTexture(gpu_device, &color_create_info), width,
                                                          height};
            }
        }
        void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        {
        }
        void compute_pass(SDL_GPUCommandBuffer *command_buffer)
        {
            event_data.lock_data_vectors();
            if (event_data.get_evt_vector_ref().empty())
            {
                event_data.unlock_data_vectors();
                return;
            }
            event_data.unlock_data_vectors();
            SDL_GPUStorageTextureReadWriteBinding texture_buffer_bindings = {0};

            texture_buffer_bindings.texture = render_targets["DigitalCodedExposure"].texture;
            texture_buffer_bindings.mip_level = 0;
            texture_buffer_bindings.layer = 0;
            texture_buffer_bindings.cycle = false;

            SDL_GPUComputePass *compute_pass =
                SDL_BeginGPUComputePass(command_buffer, &texture_buffer_bindings, 1, nullptr, 0);

            SDL_GPUBuffer *points_buffer = scrubber->get_points_buffer();
            SDL_BindGPUComputeStorageBuffers(compute_pass, 0, &points_buffer, 1);
            SDL_BindGPUComputeStorageTextures(compute_pass, 1, &render_targets["DigitalCodedExposure"].texture, 1);
            SDL_BindGPUComputePipeline(compute_pass, compute_pipeline);

            glm::vec4 color = glm::vec4(parameter_store->get<glm::vec3>("polarity_neg_color"), 1.0f);
            PassData pass_data;
            pass_data.col = color;
            bool shutter_is_positive_only = false;
            try
            {
                shutter_is_positive_only = parameter_store->get<bool>("shutter_is_positive_only");
            }
            catch (...)
            {
                std::cout << "lol" << std::endl;
            }
            glm::vec4 positiveOnly = glm::vec4(shutter_is_positive_only ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
            pass_data.posOnly = positiveOnly;
            SDL_PushGPUVertexUniformData(command_buffer, 0, &pass_data, sizeof(pass_data));
            SDL_DispatchGPUCompute(compute_pass, width, height, 1);

            SDL_EndGPUComputePass(compute_pass);
        }
        void render_pass(SDL_GPUCommandBuffer *command_buffer)
        {
        }
};

#endif