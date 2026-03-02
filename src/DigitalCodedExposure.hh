#pragma once
#ifndef DIGITAL_CODED_EXPOSURE_HH
#define DIGITAL_CODED_EXPOSURE_HH

#include "pch.hh"

#include "Camera.hh"
#include "EventData.hh"
#include "RenderTarget.hh"
#include "Scrubber.hh"
#include "UploadBuffer.hh"
#include "shaders/digital_coded_exposure/clear_comp.h"
#include "shaders/digital_coded_exposure/dce_comp.h"
#include "shaders/digital_coded_exposure/process_comp.h"

#include <iostream>

/**
 * @brief Flags passed to the compute shaders to determine how DCE is computed.
 */
struct PassData
{
        glm::vec4 posCol;     // Positive color
        glm::vec4 neutCol;    // Neutral color
        glm::vec4 negCol;     // Negative color
        glm::vec4 floatFlags; // x: color option, y: scale, z: activation function (0 for linear 1 for sigmoid), w: time
                              // center
        glm::vec4 flags;      // x: posOnly, y: morlet
        glm::vec4 morletParams; // x: frequency, y: width (h), z: time center
};

/**
 * @brief This class is responsible for creating the necessary compute pipelines to create the
 *        digital coded exposure.
 */
class DigitalCodedExposure
{
    private:
        EventData &event_data;
        Scrubber &scrubber;
        std::unordered_map<std::string, RenderTarget> &render_targets;

        SDL_Window *window = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;

        SDL_GPUComputePipeline *compute_pipeline = nullptr;
        SDL_GPUComputePipeline *clear_compute_pipeline = nullptr;
        SDL_GPUComputePipeline *process_compute_pipeline = nullptr;

        SDL_GPUTexture *positive_values_texture = nullptr;
        SDL_GPUTexture *negative_values_texture = nullptr;

        unsigned int width{};
        unsigned int height{};

        /**
         * @brief Creates intermediate texture to be used in pipeline.
         * @param width width of texture.
         * @param height height of texture.
         * @return SDL_GPUTexture pointer to intermediate texture.
         */
        SDL_GPUTexture *create_intermediate_texture(unsigned int width, unsigned int height)
        {
            SDL_GPUTextureCreateInfo color_create_info = {
                .type = SDL_GPU_TEXTURETYPE_2D,
                .format = SDL_GPU_TEXTUREFORMAT_R32_UINT,
                .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE |
                         SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE,
                .width = width,
                .height = height,
                .layer_count_or_depth = 1,
                .num_levels = 1,
                .sample_count = SDL_GPU_SAMPLECOUNT_1,
            };

            return SDL_CreateGPUTexture(gpu_device, &color_create_info);
        }

    public:

        std::shared_mutex mutex;

        float event_contrib_weight = 0.5f;
        float morlet_frequency = 0.0f;
        float morlet_width = 0.01f;

        bool texture_initialization_required = false; // Flag set by data acqusition thread after a new file/camera is loaded
        bool shutter_is_morlet = false;
        bool shutter_is_positive_only = false;
        bool combine_color = false; // Unused by phase 2, purpose unknown

        int32_t dce_color = 0;              // 0 - High/Low, 1 - Tricolor, 2 - Use same colors as visualizer
        int32_t activation_function = 0;    // 0 - Linear, 1 - Sigmoid

        glm::vec3 polarity_neg_color = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 polarity_pos_color = glm::vec3(1.0f, 1.0f, 1.0f);
        glm::vec3 polarity_neut_color = glm::vec3(0.5f, 0.5f, 0.5f);

        /**
         * @brief Constructor. Initializes compute pipelines.
         * @param event_data EventData object containing event/frame data
         * @param scrubber Scrubber object with data to compute DCE on
         * @param render_targets Render targets of the program
         * @param window SDL_Window to draw on
         * @param gpu_device SDL_GPUDevice to create texture on
         */
        DigitalCodedExposure(EventData& event_data, Scrubber& scrubber, std::unordered_map<std::string, RenderTarget>& render_targets, 
                            SDL_Window* window, SDL_GPUDevice* gpu_device): 
                            event_data(event_data), scrubber(scrubber), render_targets(render_targets), window(window), 
                            gpu_device(gpu_device), width{}, height{}
        {
            // create the color texture, this is the texture that will store the color data
            SDL_GPUTextureCreateInfo color_create_info = {
                .type = SDL_GPU_TEXTURETYPE_2D,
                .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
                .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE,
                .width = 1920,
                .height = 1200,
                .layer_count_or_depth = 1,
                .num_levels = 1,
                .sample_count = SDL_GPU_SAMPLECOUNT_1,
            };
            render_targets["DigitalCodedExposure"] = {SDL_CreateGPUTexture(gpu_device, &color_create_info), 1920, 1080};

            positive_values_texture = create_intermediate_texture(1920, 1080);
            negative_values_texture = create_intermediate_texture(1920, 1080);

            SDL_GPUComputePipelineCreateInfo clear_compute_pipeline_info = {0};
            clear_compute_pipeline_info.code_size = sizeof(clear_comp);
            clear_compute_pipeline_info.code = (Uint8 *)clear_comp;
            clear_compute_pipeline_info.entrypoint = "main";
            clear_compute_pipeline_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            clear_compute_pipeline_info.num_samplers = 0;
            clear_compute_pipeline_info.num_readonly_storage_textures = 0;
            clear_compute_pipeline_info.num_readonly_storage_buffers = 0;
            clear_compute_pipeline_info.num_readwrite_storage_textures = 3;
            clear_compute_pipeline_info.num_readwrite_storage_buffers = 0;
            clear_compute_pipeline_info.num_uniform_buffers = 0;
            clear_compute_pipeline_info.threadcount_x = 1;
            clear_compute_pipeline_info.threadcount_y = 1;
            clear_compute_pipeline_info.threadcount_z = 1;

            clear_compute_pipeline = SDL_CreateGPUComputePipeline(gpu_device, &clear_compute_pipeline_info);

            SDL_GPUComputePipelineCreateInfo compute_pipeline_info = {0};
            compute_pipeline_info.code_size = sizeof(dce_comp);
            compute_pipeline_info.code = (Uint8 *)dce_comp;
            compute_pipeline_info.entrypoint = "main";
            compute_pipeline_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            compute_pipeline_info.num_samplers = 0;
            compute_pipeline_info.num_readonly_storage_textures = 0;
            compute_pipeline_info.num_readonly_storage_buffers = 1;
            compute_pipeline_info.num_readwrite_storage_textures = 3;
            compute_pipeline_info.num_readwrite_storage_buffers = 0;
            compute_pipeline_info.num_uniform_buffers = 1;
            compute_pipeline_info.threadcount_x = 1;
            compute_pipeline_info.threadcount_y = 1;
            compute_pipeline_info.threadcount_z = 1;

            compute_pipeline = SDL_CreateGPUComputePipeline(gpu_device, &compute_pipeline_info);

            SDL_GPUComputePipelineCreateInfo process_compute_pipeline_info = {0};
            process_compute_pipeline_info.code_size = sizeof(process_comp);
            process_compute_pipeline_info.code = (Uint8 *)process_comp;
            process_compute_pipeline_info.entrypoint = "main";
            process_compute_pipeline_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            process_compute_pipeline_info.num_samplers = 0;
            process_compute_pipeline_info.num_readonly_storage_textures = 0;
            process_compute_pipeline_info.num_readonly_storage_buffers = 0;
            process_compute_pipeline_info.num_readwrite_storage_textures = 3;
            process_compute_pipeline_info.num_readwrite_storage_buffers = 0;
            process_compute_pipeline_info.num_uniform_buffers = 1;
            process_compute_pipeline_info.threadcount_x = 1;
            process_compute_pipeline_info.threadcount_y = 1;
            process_compute_pipeline_info.threadcount_z = 1;

            process_compute_pipeline = SDL_CreateGPUComputePipeline(gpu_device, &process_compute_pipeline_info);
        }

        /**
         * @brief Destructor. Releases textures and pipelines related to Digital Coded Exposure.
         */
        ~DigitalCodedExposure()
        {
            SDL_ReleaseGPUComputePipeline(gpu_device, compute_pipeline);
            SDL_ReleaseGPUComputePipeline(gpu_device, clear_compute_pipeline);
            SDL_ReleaseGPUComputePipeline(gpu_device, process_compute_pipeline);

            SDL_ReleaseGPUTexture(gpu_device, render_targets["DigitalCodedExposure"].texture);
            SDL_ReleaseGPUTexture(gpu_device, positive_values_texture);
            SDL_ReleaseGPUTexture(gpu_device, negative_values_texture);
        }

        /**
         * @brief Unimplemented event handler function for digital coded exposure. For future functionality.
         */
        bool event_handler(SDL_Event *event)
        {
            return false;
        }

        /**
         * @brief Called to update Digital Coded Exposure every frame.
         *        Recreates texture should file change.
         */
        void cpu_update()
        {
            event_data.lock_data_vectors();
            if (event_data.get_evt_vector_ref().empty())
            {
                event_data.unlock_data_vectors();
                // Delete old texture
                if (render_targets["DigitalCodedExposure"].texture)
                {
                    SDL_ReleaseGPUTexture(gpu_device, render_targets["DigitalCodedExposure"].texture);
                    render_targets["DigitalCodedExposure"].texture = nullptr;
                }

                return;
            }
            event_data.unlock_data_vectors();

            // Only generate textures when a new file has been loaded with new resolution
            std::unique_lock dce_read_write_lock(mutex);
            if (texture_initialization_required)
            {        
                // This only needs to happen once when threads.hh loads a new file/camera so set back to false
                texture_initialization_required = false;
                dce_read_write_lock.unlock();

                width = event_data.get_camera_event_resolution().x;
                height = event_data.get_camera_event_resolution().y;

                // Sanity check
                if (width == 0 || height == 0 || width > 1920.0f || height > 1200.0f)
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

                SDL_ReleaseGPUTexture(gpu_device, positive_values_texture);
                SDL_ReleaseGPUTexture(gpu_device, negative_values_texture);

                positive_values_texture = create_intermediate_texture(width, height);
                negative_values_texture = create_intermediate_texture(width, height);
            }
        }

        /**
         * @brief Does nothing atm
         */
        void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        {
        }

        /**
         * @brief Compute pass. Dispatches compute shaders to calculate Digital Coded Exposure output.
         * @param command_buffer GPU command buffer.
         */
        void compute_pass(SDL_GPUCommandBuffer *command_buffer)
        {
            // Ensure there is data
            event_data.lock_data_vectors();
            if (event_data.get_evt_vector_ref().empty())
            {
                event_data.unlock_data_vectors();
                return;
            }
            event_data.unlock_data_vectors();
            // Sanity check resolution
            if (width == 0.0f || height == 0.0f || width > 1920.0f || height > 1200.0f)
            {
                return;
            }

            SDL_GPUStorageTextureReadWriteBinding texture_buffer_bindings[3] = {0};

            // First texture: DigitalCodedExposure
            texture_buffer_bindings[0].texture = render_targets["DigitalCodedExposure"].texture;
            texture_buffer_bindings[0].mip_level = 0;
            texture_buffer_bindings[0].layer = 0;
            texture_buffer_bindings[0].cycle = false;

            texture_buffer_bindings[1].texture = positive_values_texture;
            texture_buffer_bindings[1].mip_level = 0;
            texture_buffer_bindings[1].layer = 0;
            texture_buffer_bindings[1].cycle = false;

            texture_buffer_bindings[2].texture = negative_values_texture;
            texture_buffer_bindings[2].mip_level = 0;
            texture_buffer_bindings[2].layer = 0;
            texture_buffer_bindings[2].cycle = false;

            SDL_GPUComputePass *compute_pass = SDL_BeginGPUComputePass(command_buffer, texture_buffer_bindings, 3, nullptr, 0);
            SDL_BindGPUComputePipeline(compute_pass, clear_compute_pipeline);
            SDL_DispatchGPUCompute(compute_pass, width, height, 1);

            SDL_GPUBuffer *points_buffer = scrubber.get_points_buffer();
            if (!points_buffer) // Points buffer is null due to racing - Phase 2,  🗿 - Phase 3
            {
                SDL_EndGPUComputePass(compute_pass);
                return;
            }

            int point_count = scrubber.get_points_buffer_size();

            SDL_BindGPUComputeStorageBuffers(compute_pass, 0, &points_buffer, 1);
            SDL_BindGPUComputePipeline(compute_pass, compute_pipeline);


            // Read current time from scrubber 
            std::shared_lock scrubber_read_lock(scrubber.mutex);
            float current_time = scrubber.current_time;
            float lower_time = scrubber.lower_time;
            float time_center = (current_time + lower_time) / 2000.0f;
            scrubber_read_lock.unlock();


            // Lock dce while reading parameters
            std::shared_lock dce_read_lock(mutex);

            // Shader parameters
            glm::vec4 floatFlags = glm::vec4(static_cast<float>(dce_color), event_contrib_weight, static_cast<float>(activation_function), 0.0f);
            glm::vec4 flags = glm::vec4((shutter_is_positive_only ? 1.0f : 0.0f), (shutter_is_morlet ? 1.0f : 0.0f), 0.0f, 0.0f);
            glm::vec4 morletParams = glm::vec4(morlet_frequency, morlet_width, time_center, 0.0f); 


            // Set colors 
            glm::vec4 negCol = glm::vec4(polarity_neg_color, 1.0f);
            glm::vec4 neutCol = glm::vec4(polarity_neut_color, 1.0f);
            glm::vec4 posCol = glm::vec4(polarity_pos_color, 1.0f);
            

            // Unlock dce before starting GPU call
            dce_read_lock.unlock();


            // GPU shit 
            PassData pass_data;
            pass_data.posCol = posCol;
            pass_data.neutCol = neutCol;
            pass_data.negCol = negCol;
            pass_data.floatFlags = floatFlags;
            pass_data.flags = flags;
            pass_data.morletParams = morletParams;
            SDL_PushGPUComputeUniformData(command_buffer, 0, &pass_data, sizeof(pass_data));
            SDL_DispatchGPUCompute(compute_pass, point_count, 1, 1);

            SDL_BindGPUComputePipeline(compute_pass, process_compute_pipeline);
            SDL_DispatchGPUCompute(compute_pass, width, height, 1);

            SDL_EndGPUComputePass(compute_pass);
        }

        void render_pass(SDL_GPUCommandBuffer *command_buffer)
        {
        }
};

#endif
