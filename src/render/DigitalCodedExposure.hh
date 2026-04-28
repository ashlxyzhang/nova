#pragma once
#ifndef DIGITAL_CODED_EXPOSURE_HH
#define DIGITAL_CODED_EXPOSURE_HH

#include "util/pch.hh"
#include "data/EventData.hh"
#include "render/Camera.hh"
#include "render/RenderTarget.hh"
#include "render/GPUDevice.hh"
#include "ui/Scrubber.hh"
#include "util/ErrorQueue.hh"
#include <memory>
struct DataSource;

#include "shaders/digital_coded_exposure/clear_comp.h"
#include "shaders/digital_coded_exposure/dce_comp.h"
#include "shaders/digital_coded_exposure/process_comp.h"


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
    public:
        struct RenderTargets
        {
            RenderTarget output;
            RenderTarget positive_values;
            RenderTarget negative_values;

            void init_textures(SDL_GPUDevice* gpu_device, cv::Size resolution) { 
                SDL_GPUTextureCreateInfo dce_create_info = {
                    .type = SDL_GPU_TEXTURETYPE_2D,
                    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE,
                    .width = (Uint32) resolution.width,
                    .height = (Uint32) resolution.height,
                    .layer_count_or_depth = 1,
                    .num_levels = 1,
                    .sample_count = SDL_GPU_SAMPLECOUNT_1,
                };
                output = {SDL_CreateGPUTexture(gpu_device, &dce_create_info), dce_create_info.width, dce_create_info.height};

                SDL_GPUTextureCreateInfo dce_intermediate_create_info = {
                    .type = SDL_GPU_TEXTURETYPE_2D,
                    .format = SDL_GPU_TEXTUREFORMAT_R32_UINT,
                    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE |
                                SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE,
                    .width = (Uint32) resolution.width,
                    .height = (Uint32) resolution.height,
                    .layer_count_or_depth = 1,
                    .num_levels = 1,
                    .sample_count = SDL_GPU_SAMPLECOUNT_1,
                };
                positive_values = {SDL_CreateGPUTexture(gpu_device, &dce_intermediate_create_info), dce_intermediate_create_info.width, dce_intermediate_create_info.height};
                negative_values = {SDL_CreateGPUTexture(gpu_device, &dce_intermediate_create_info), dce_intermediate_create_info.width, dce_intermediate_create_info.height};
            }
            
            void delete_textures(SDL_GPUDevice* gpu_device) {
                SDL_ReleaseGPUTexture(gpu_device, output.texture);
                SDL_ReleaseGPUTexture(gpu_device, positive_values.texture);
                SDL_ReleaseGPUTexture(gpu_device, negative_values.texture);
            }
        };

        struct Parameters
        {
            float event_contrib_weight = 0.5f;
            float morlet_frequency = 0.0f;
            float morlet_width = 0.01f;

            bool shutter_is_morlet = false;
            bool shutter_is_positive_only = false;
            bool combine_color = false;

            int32_t dce_color = 0;           // 0 - High/Low, 1 - Tricolor, 2 - Use same colors as visualizer
            int32_t activation_function = 0; // 0 - Linear, 1 - Sigmoid

            glm::vec3 polarity_neg_color = glm::vec3(0.0f, 0.0f, 0.0f);
            glm::vec3 polarity_pos_color = glm::vec3(1.0f, 1.0f, 1.0f);
            glm::vec3 polarity_neut_color = glm::vec3(0.5f, 0.5f, 0.5f);
        };

    private:

        // GPU
        SDL_GPUDevice *gpu_device = nullptr;

        SDL_GPUComputePipeline *compute_pipeline = nullptr;
        SDL_GPUComputePipeline *clear_compute_pipeline = nullptr;
        SDL_GPUComputePipeline *process_compute_pipeline = nullptr;

    public:
        /**
         * @brief Constructor. Initializes compute pipelines.
         * @param gpu_device SDL_GPUDevice to create texture on
         */
        DigitalCodedExposure(GPUDevice& gpu_device)
            : gpu_device(gpu_device.get_SDL_device())
        {

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

            clear_compute_pipeline = SDL_CreateGPUComputePipeline(this->gpu_device, &clear_compute_pipeline_info);

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

            compute_pipeline = SDL_CreateGPUComputePipeline(this->gpu_device, &compute_pipeline_info);

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

            process_compute_pipeline = SDL_CreateGPUComputePipeline(this->gpu_device, &process_compute_pipeline_info);
        }

        /**
         * @brief Destructor. Releases textures and pipelines related to Digital Coded Exposure.
         */
        ~DigitalCodedExposure()
        {
            SDL_ReleaseGPUComputePipeline(gpu_device, compute_pipeline);
            SDL_ReleaseGPUComputePipeline(gpu_device, clear_compute_pipeline);
            SDL_ReleaseGPUComputePipeline(gpu_device, process_compute_pipeline);
        }

        /**
         * @brief Compute pass. Dispatches compute shaders to calculate Digital Coded Exposure output.
         * @param command_buffer GPU command buffer.
         */
        void render(std::shared_ptr<DataSource> data_source);
        void render(DataSource& data_source);
};

#endif
