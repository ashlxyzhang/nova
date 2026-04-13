#pragma once
#ifndef DIGITAL_CODED_EXPOSURE_HH
#define DIGITAL_CODED_EXPOSURE_HH

#include "util/pch.hh"

#include "data/DataAcquisition.hh"
#include "data/EventData.hh"
#include "render/Camera.hh"
#include "render/RenderTarget.hh"
#include "render/UploadBuffer.hh"
#include "ui/Scrubber.hh"
#include "util/ErrorQueue.hh"
#include "data/DataSource.hh"

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
         * @param data_acq DataAcquisition object used to access event data for processing
         * @param gpu_device SDL_GPUDevice to create texture on
         */
        DigitalCodedExposure(SDL_GPUDevice *gpu_device)
            : gpu_device(gpu_device)
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
        }

        /**
         * @brief Compute pass. Dispatches compute shaders to calculate Digital Coded Exposure output.
         * @param command_buffer GPU command buffer.
         */
        void render(std::shared_ptr<DataSource> data_source)
        {   
            // Read parameters, rendering textures, and resolution from data source
            Parameters params = data_source->dce_parameters;
            RenderTargets render_targets = data_source->dce_render_targets;
            cv::Size resolution = data_source->resolution;


            // Format parameters to send to the GPU
            PassData pass_data;
            pass_data.posCol = glm::vec4(params.polarity_pos_color, 1.0f);
            pass_data.neutCol = glm::vec4(params.polarity_neut_color, 1.0f);
            pass_data.negCol = glm::vec4(params.polarity_neg_color, 1.0f);
            pass_data.floatFlags = glm::vec4(static_cast<float>(params.dce_color), params.event_contrib_weight,
                                             static_cast<float>(params.activation_function), 0.0f);
            pass_data.flags = glm::vec4((params.shutter_is_positive_only ? 1.0f : 0.0f),
                                        (params.shutter_is_morlet ? 1.0f : 0.0f), 0.0f, 0.0f);
            pass_data.morletParams = glm::vec4(params.morlet_frequency, params.morlet_width, 0.0f, 0.0f);


            // Format textures to send to the GPU
            SDL_GPUStorageTextureReadWriteBinding texture_buffer_bindings[3] = {0};

            texture_buffer_bindings[0].texture = render_targets.output.texture;
            texture_buffer_bindings[0].mip_level = 0;
            texture_buffer_bindings[0].layer = 0;
            texture_buffer_bindings[0].cycle = false;

            texture_buffer_bindings[1].texture = render_targets.positive_values.texture;
            texture_buffer_bindings[1].mip_level = 0;
            texture_buffer_bindings[1].layer = 0;
            texture_buffer_bindings[1].cycle = false;

            texture_buffer_bindings[2].texture = render_targets.negative_values.texture;
            texture_buffer_bindings[2].mip_level = 0;
            texture_buffer_bindings[2].layer = 0;
            texture_buffer_bindings[2].cycle = false;



            // Begin GPU passes
            SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);

            // --- Pass A: Clear all textures ---
            SDL_GPUComputePass *clear_pass = SDL_BeginGPUComputePass(command_buffer, texture_buffer_bindings, 3, nullptr, 0);
            SDL_BindGPUComputePipeline(clear_pass, clear_compute_pipeline);
            SDL_DispatchGPUCompute(clear_pass, resolution.width, resolution.height, 1);
            SDL_EndGPUComputePass(clear_pass);

            // Get points buffer; run passes B and C only if there are points to process
            SDL_GPUBuffer *points_buffer = data_source->scrubber.get_points_buffer();
            int point_count = data_source->scrubber.get_points_buffer_size();

            if (points_buffer && point_count > 0)
            {
                // Calculate time_center from scrubber and pass using pass_data.morletParams.z (see dce.comp for usage
                // in shader)
                Scrubber::State scrubber_state = data_source->scrubber.get_state();

                float time_center = (scrubber_state.current_time + scrubber_state.lower_time) / 2000.0f;
                pass_data.morletParams.z = time_center;

                // --- Pass B: Accumulate events into intermediate textures ---
                SDL_GPUComputePass *dce_pass =
                    SDL_BeginGPUComputePass(command_buffer, texture_buffer_bindings, 3, nullptr, 0);
                SDL_BindGPUComputePipeline(dce_pass, compute_pipeline);
                SDL_BindGPUComputeStorageBuffers(dce_pass, 0, &points_buffer, 1);
                SDL_PushGPUComputeUniformData(command_buffer, 0, &pass_data, sizeof(pass_data));
                SDL_DispatchGPUCompute(dce_pass, point_count, 1, 1);
                SDL_EndGPUComputePass(dce_pass);

                // --- Pass C: Process intermediate textures into final output ---
                SDL_GPUComputePass *process_pass =
                    SDL_BeginGPUComputePass(command_buffer, texture_buffer_bindings, 3, nullptr, 0);
                SDL_BindGPUComputePipeline(process_pass, process_compute_pipeline);
                SDL_PushGPUComputeUniformData(command_buffer, 0, &pass_data, sizeof(pass_data));
                SDL_DispatchGPUCompute(process_pass, resolution.width, resolution.height, 1);
                SDL_EndGPUComputePass(process_pass);
            }

            SDL_SubmitGPUCommandBuffer(command_buffer);
            SDL_WaitForGPUIdle(gpu_device);
        }
};

#endif
