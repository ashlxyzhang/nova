#include "render/DigitalCodedExposure.hh"
#include "data/DataSource.hh"

void DigitalCodedExposure::render(DataSource& data_source)
{
    // Read parameters, rendering textures, and resolution from data source
    Parameters params = data_source.dce_parameters;
    RenderTargets render_targets = data_source.dce_render_targets;
    cv::Size resolution = data_source.get_resolution();


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
    SDL_GPUBuffer *points_buffer = data_source.scrubber.get_points_buffer();
    int point_count = data_source.scrubber.get_points_buffer_size();
    
    if (points_buffer && point_count > 0)
    {
        // Calculate time_center from scrubber and pass using pass_data.morletParams.z (see dce.comp for usage
        // in shader)
        Scrubber::State scrubber_state = data_source.scrubber.state;

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

    // Stalls CPU until this particular command is finished
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    SDL_WaitForGPUFences(gpu_device, true, &fence, 1);
    SDL_ReleaseGPUFence(gpu_device, fence);
}

void DigitalCodedExposure::render(std::shared_ptr<DataSource> data_source) {
    render(*data_source);
}