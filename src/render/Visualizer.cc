#include "render/Visualizer.hh"
#include "data/DataSource.hh"

namespace nova {


void Visualizer::PointsRenderer::render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                             const glm::mat4 &vp, DataSource& data_source,
                                             const Parameters &params)
{
    if (data_source.scrubber.get_points_buffer_size() == 0)
        return;

    SDL_BindGPUGraphicsPipeline(render_pass, points_pipeline);

    SDL_GPUBuffer* points_buffer = data_source.scrubber.get_points_buffer();
    SDL_GPUBufferBinding vertex_buffer_binding;
    vertex_buffer_binding.buffer = points_buffer;
    vertex_buffer_binding.offset = 0;
    SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_buffer_binding, 1);

    struct PointsUniforms
    {
            glm::mat4 mvp;
            glm::vec4 negative_color;
            glm::vec4 positive_color;
            float point_size;
    } uniforms;

    glm::vec2 camera_resolution = data_source.scrubber.get_camera_resolution();
    float lower_depth = data_source.scrubber.get_lower_depth();
    float upper_depth = data_source.scrubber.get_upper_depth();
    float depth_range = upper_depth - lower_depth;

    glm::mat4 z_translate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -lower_depth));
    glm::mat4 scale_matrix = glm::scale(
        glm::mat4(1.0f), glm::vec3(2.0f / camera_resolution.x, 2.0f / camera_resolution.y, 2.0f / depth_range));
    glm::mat4 translate_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, -1.0f, -1.0f));
    glm::mat4 rotate_matrix = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 z_switch = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 reflect_yz = glm::scale(glm::mat4(1.0f), glm::vec3(-1.0f, 1.0f, 1.0f));

    uniforms.mvp = vp * reflect_yz * z_switch * rotate_matrix * translate_matrix * scale_matrix * z_translate;

    uniforms.negative_color = glm::vec4(params.polarity_neg_color, 1.0f);
    uniforms.positive_color = glm::vec4(params.polarity_pos_color, 1.0f);
    uniforms.point_size = params.particle_scale;

    SDL_PushGPUVertexUniformData(command_buffer, 0, &uniforms, sizeof(uniforms));

    SDL_DrawGPUPrimitives(render_pass, data_source.scrubber.get_points_buffer_size(), 1, 0, 0);
}

void Visualizer::TextRenderer::cpu_update(DataSource& data_source, const Parameters &params)
{
    vertices.clear();
    indices.clear();
    draw_calls.clear();

    for (TTF_Text *text_obj : managed_text_objects)
    {
        TTF_DestroyText(text_obj);
    }
    managed_text_objects.clear();

    float lower_depth = data_source.scrubber.get_lower_depth();
    float upper_depth = data_source.scrubber.get_upper_depth();
    float depth_range = upper_depth - lower_depth;

    glm::vec3 text_position = {1.0f, -1.0f, 0.0f};
    glm::vec3 text_normal = {1.0f, 0.0f, 0.0f};
    SDL_FColor text_color = {0.0f, 0.0f, 0.0f, 1.0f};

    for (uint32_t i = 0; i <= params.grid_z_subdivisions; ++i)
    {
        float normalized_z = 2.0f * static_cast<float>(i) / static_cast<float>(params.grid_z_subdivisions) - 1.0f;

        float timestamp = lower_depth + (normalized_z + 1.0f) * 0.5f * depth_range;

        std::string timestamp_str{};
        switch (params.unit_type)
        {
        case TIME::UNIT_US:
            timestamp_str = std::format("{:.2f}", timestamp / params.unit_time_conversion_factor);
            break;
        case TIME::UNIT_MS:
            timestamp_str = std::format("{:.4f}", timestamp / params.unit_time_conversion_factor);
            break;
        case TIME::UNIT_S:
            timestamp_str = std::format("{:.8f}", timestamp / params.unit_time_conversion_factor);
            break;
        }

        text_position.z = normalized_z;
        add_text(timestamp_str, text_position, text_normal, text_color);
    }
}

void Visualizer::SlamRenderer::render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                           const glm::mat4 &vp, const Parameters &params)
{
    if (!slam_pipeline || !vertex_buffer || vertices.empty())
        return;

    SDL_BindGPUGraphicsPipeline(render_pass, slam_pipeline);

    SDL_GPUBufferBinding binding;
    binding.buffer = vertex_buffer;
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(render_pass, 0, &binding, 1);

    struct Uniforms
    {
            glm::mat4 mvp;
            float point_size;
    } uniforms;
    uniforms.mvp = vp;
    uniforms.point_size = params.particle_scale;

    SDL_PushGPUVertexUniformData(command_buffer, 0, &uniforms, sizeof(uniforms));
    SDL_DrawGPUPrimitives(render_pass, static_cast<Uint32>(vertices.size()), 1, 0, 0);
}

void Visualizer::FramesRenderer::render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                             const glm::mat4 &vp, DataSource& data_source,
                                             const Parameters &params)
{
    if (!frames_pipeline || data_source.scrubber.get_frames_timestamps()[0] < 0.0f)
    {
        return;
    }

    SDL_BindGPUGraphicsPipeline(render_pass, frames_pipeline);

    glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 mvp = vp * rotate;
    SDL_PushGPUVertexUniformData(command_buffer, 0, &mvp[0][0], sizeof(mvp));

    SDL_GPUTextureSamplerBinding sampler_binding = {};
    sampler_binding.texture = data_source.scrubber.get_frames_texture();
    sampler_binding.sampler = sampler;
    SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);

    glm::vec4 frame_data = {data_source.scrubber.get_frames_timestamps()[0],
                           data_source.scrubber.get_frames_timestamps()[1],
                           data_source.scrubber.get_upper_depth(), 0.0f};
    SDL_PushGPUFragmentUniformData(command_buffer, 0, &frame_data, sizeof(frame_data));

    SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);
}

void Visualizer::render(std::shared_ptr<DataSource> data_source) {
    render(*data_source);
}

void Visualizer::render(DataSource& data_source)
{
    Parameters params = data_source.visualizer_parameters;
    RenderTargets render_targets = data_source.visualizer_render_targets;

    bool slam_active = slam_pointcloud_ != nullptr;

    // CPU Update phase
    if (!slam_active)
    {
        // Move camera back to default location in case it moved around during SLAM
        camera.setOrbitCenter(glm::vec3(0,0,0));
        slam_renderer->clear();
        grid_renderer->cpu_update(params);
        points_renderer->cpu_update(data_source, params);
        text_renderer->cpu_update(data_source, params);
        frames_renderer->cpu_update(data_source, params);
    }
    else if(params.is_left_camera)
    {
        if(params.display_global_pointcloud)
        {
            if(slam_pc_changed)
                slam_renderer->cpu_update_global(slam_global_pointcloud_);
            if(slam_path_changed)
                slam_renderer->cpu_update_path(slam_path_);
        }
        else
        {
            if(slam_pc_changed)
                slam_renderer->cpu_update(slam_pointcloud_);
        }
    }

    // Create command buffer and copy pass once for all sub-renderers
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);

    // Copy pass phase
    if (!slam_active)
    {
        grid_renderer->copy_pass(transfer_buffer, copy_pass);
        points_renderer->copy_pass(transfer_buffer, copy_pass, data_source);
        text_renderer->copy_pass(transfer_buffer, copy_pass, data_source);
        frames_renderer->copy_pass(transfer_buffer, copy_pass, data_source);
    }
    else if(params.is_left_camera)
    {
        if((slam_pc_changed))
            slam_renderer->copy_pass(transfer_buffer, copy_pass);
        if(slam_path_changed)
            slam_renderer->copy_pass_path(transfer_buffer, copy_pass);
    }

    // End copy pass
    SDL_EndGPUCopyPass(copy_pass);

    // Render pass phase
    SDL_GPUColorTargetInfo color_target_info = {0};
    color_target_info.texture = render_targets.color.texture;
    SDL_FColor color = {1.0f, 1.0f, 1.0f, 1.0f};
    color_target_info.clear_color = color;
    color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;
    color_target_info.mip_level = 0;
    color_target_info.layer_or_depth_plane = 0;
    color_target_info.cycle = false;

    SDL_GPUDepthStencilTargetInfo depth_target_info = {0};
    depth_target_info.texture = render_targets.depth.texture;
    depth_target_info.clear_depth = 1.0f;
    depth_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_target_info.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth_target_info.cycle = false;

    SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, &depth_target_info);

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix();
    glm::mat4 vp = projection * view;

    if (!slam_active)
    {
        grid_renderer->render_pass(command_buffer, render_pass, vp);
        points_renderer->render_pass(command_buffer, render_pass, vp, data_source, params);
        frames_renderer->render_pass(command_buffer, render_pass, vp, data_source, params);
        text_renderer->render_pass(command_buffer, render_pass, vp, data_source, params);
        if (params.show_oscilloscope)
        {
            float z1 = params.osc_t1 * 2.0f - 1.0f;
            float z2 = params.osc_t2 * 2.0f - 1.0f;
            glm::vec4 glass = glm::vec4(0.3f, 0.6f, 1.0f, 0.35f);
            std::vector<std::pair<glm::mat4, glm::vec4>> rects = {
                {vp * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, z1)), glass},
                {vp * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, z2)), glass},
            };
            osc_renderer->render_pass(command_buffer, render_pass, rects);
        }
    }
    else if(params.is_left_camera)
    {
        slam_renderer->render_pass(command_buffer, render_pass, vp, params);
        // Only show path if displaying the global point cloud
        if(params.display_global_pointcloud)
            slam_renderer->render_pass_path(command_buffer, render_pass, vp, params);
    }

    SDL_EndGPURenderPass(render_pass);

    // Stalls CPU until this particular command is finished
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    SDL_WaitForGPUFences(gpu_device, true, &fence, 1);
    SDL_ReleaseGPUFence(gpu_device, fence);
}

} // namespace nova
