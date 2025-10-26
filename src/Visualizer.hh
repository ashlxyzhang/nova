#pragma once

#ifndef VISUALIZER_HH
#define VISUALIZER_HH

#include "pch.hh"

#include "Camera.hh"
#include "EventData.hh"
#include "ParameterStore.hh"
#include "RenderTarget.hh"
#include "UploadBuffer.hh"

#include "shaders/visualizer/grid/grid_frag.h"
#include "shaders/visualizer/grid/grid_vert.h"
#include "shaders/visualizer/points/points_frag.h"
#include "shaders/visualizer/points/points_vert.h"

class Visualizer
{
    private:
        class GridRenderer
        {
            private:
                uint32_t x_subdivisions = 5;
                uint32_t y_subdivisions = 5;
                uint32_t z_subdivisions = 5;
                std::vector<glm::vec3> lines;

                ParameterStore &parameter_store;
                SDL_GPUDevice *gpu_device = nullptr;
                SDL_GPUGraphicsPipeline *grid_pipeline = nullptr;
                SDL_GPUBuffer *vertex_buffer = nullptr;

                void generate_grid_lines()
                {
                    lines.clear();

                    // Generate lines only on the three outside faces:
                    // 1. Back face (Z = -1.0f)
                    // 2. Bottom face (Y = -1.0f) 
                    // 3. Left face (X = -1.0f)

                    // Back face (Z = -1.0f) - lines parallel to X and Y axes
                    for (uint32_t i = 0; i <= x_subdivisions; ++i)
                    {
                        float x = 2.0f * static_cast<float>(i) / static_cast<float>(x_subdivisions) - 1.0f;
                        // Lines parallel to Y-axis on back face
                        lines.push_back(glm::vec3(x, -1.0f, -1.0f));
                        lines.push_back(glm::vec3(x, 1.0f, -1.0f));
                    }
                    for (uint32_t i = 0; i <= y_subdivisions; ++i)
                    {
                        float y = 2.0f * static_cast<float>(i) / static_cast<float>(y_subdivisions) - 1.0f;
                        // Lines parallel to X-axis on back face
                        lines.push_back(glm::vec3(-1.0f, y, -1.0f));
                        lines.push_back(glm::vec3(1.0f, y, -1.0f));
                    }

                    // Bottom face (Y = -1.0f) - lines parallel to X and Z axes
                    for (uint32_t i = 0; i <= x_subdivisions; ++i)
                    {
                        float x = 2.0f * static_cast<float>(i) / static_cast<float>(x_subdivisions) - 1.0f;
                        // Lines parallel to Z-axis on bottom face
                        lines.push_back(glm::vec3(x, -1.0f, -1.0f));
                        lines.push_back(glm::vec3(x, -1.0f, 1.0f));
                    }
                    for (uint32_t i = 0; i <= z_subdivisions; ++i)
                    {
                        float z = 2.0f * static_cast<float>(i) / static_cast<float>(z_subdivisions) - 1.0f;
                        // Lines parallel to X-axis on bottom face
                        lines.push_back(glm::vec3(-1.0f, -1.0f, z));
                        lines.push_back(glm::vec3(1.0f, -1.0f, z));
                    }

                    // Left face (X = -1.0f) - lines parallel to Y and Z axes
                    for (uint32_t i = 0; i <= y_subdivisions; ++i)
                    {
                        float y = 2.0f * static_cast<float>(i) / static_cast<float>(y_subdivisions) - 1.0f;
                        // Lines parallel to Z-axis on left face
                        lines.push_back(glm::vec3(-1.0f, y, -1.0f));
                        lines.push_back(glm::vec3(-1.0f, y, 1.0f));
                    }
                    for (uint32_t i = 0; i <= z_subdivisions; ++i)
                    {
                        float z = 2.0f * static_cast<float>(i) / static_cast<float>(z_subdivisions) - 1.0f;
                        // Lines parallel to Y-axis on left face
                        lines.push_back(glm::vec3(-1.0f, -1.0f, z));
                        lines.push_back(glm::vec3(-1.0f, 1.0f, z));
                    }
                }

            public:
                GridRenderer(ParameterStore &parameter_store, SDL_GPUDevice *gpu_device, UploadBuffer *upload_buffer,
                             SDL_GPUCopyPass *copy_pass)
                    : parameter_store(parameter_store), gpu_device(gpu_device)
                {
                    if (parameter_store.exists("visualizer.grid.x_subdivisions"))
                    {
                        x_subdivisions = parameter_store.get<uint32_t>("visualizer.grid.x_subdivisions");
                    }
                    else
                    {
                        parameter_store.add<uint32_t>("visualizer.grid.x_subdivisions", x_subdivisions);
                    }

                    if (parameter_store.exists("visualizer.grid.y_subdivisions"))
                    {
                        y_subdivisions = parameter_store.get<uint32_t>("visualizer.grid.y_subdivisions");
                    }
                    else
                    {
                        parameter_store.add<uint32_t>("visualizer.grid.y_subdivisions", y_subdivisions);
                    }

                    if (parameter_store.exists("visualizer.grid.z_subdivisions"))
                    {
                        z_subdivisions = parameter_store.get<uint32_t>("visualizer.grid.z_subdivisions");
                    }
                    else
                    {
                        parameter_store.add<uint32_t>("visualizer.grid.z_subdivisions", z_subdivisions);
                    }

                    generate_grid_lines();

                    SDL_GPUShaderCreateInfo vs_create_info = {0};
                    vs_create_info.code_size = sizeof grid_vert;
                    vs_create_info.code = (const unsigned char *)grid_vert;
                    vs_create_info.entrypoint = "main";
                    vs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
                    vs_create_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
                    vs_create_info.num_samplers = 0;
                    vs_create_info.num_storage_textures = 0;
                    vs_create_info.num_storage_buffers = 0;
                    vs_create_info.num_uniform_buffers = 1;
                    SDL_GPUShader *vs = SDL_CreateGPUShader(gpu_device, &vs_create_info);

                    SDL_GPUShaderCreateInfo fs_create_info = {0};
                    fs_create_info.code_size = sizeof grid_frag;
                    fs_create_info.code = (const unsigned char *)grid_frag;
                    fs_create_info.entrypoint = "main";
                    fs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
                    fs_create_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
                    fs_create_info.num_samplers = 0;
                    fs_create_info.num_storage_textures = 0;
                    fs_create_info.num_storage_buffers = 0;
                    fs_create_info.num_uniform_buffers = 0;
                    SDL_GPUShader *fs = SDL_CreateGPUShader(gpu_device, &fs_create_info);

                    // Create vertex buffer for grid lines
                    SDL_GPUBufferCreateInfo vertex_buffer_create_info = {0};
                    vertex_buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
                    vertex_buffer_create_info.size = lines.size() * sizeof(glm::vec3);
                    vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &vertex_buffer_create_info);

                    // Upload grid lines data to GPU
                    upload_buffer->upload_to_gpu(copy_pass, vertex_buffer, lines.data(),
                                                 lines.size() * sizeof(glm::vec3));

                    // Create graphics pipeline for line rendering
                    SDL_GPUVertexBufferDescription vertex_buffer_desc = {0, sizeof(glm::vec3),
                                                                         SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
                    SDL_GPUVertexAttribute vertex_attribute = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0};
                    SDL_GPUColorTargetDescription color_target_desc = {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM};

                    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
                        .vertex_shader = vs,
                        .fragment_shader = fs,
                        .vertex_input_state = {.vertex_buffer_descriptions = &vertex_buffer_desc,
                                               .num_vertex_buffers = 1,
                                               .vertex_attributes = &vertex_attribute,
                                               .num_vertex_attributes = 1},
                        .primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST,
                        .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                                                .enable_depth_test = true,
                                                .enable_depth_write = true},
                        .target_info = {.color_target_descriptions = &color_target_desc,
                                        .num_color_targets = 1,
                                        .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                                        .has_depth_stencil_target = true}};

                    grid_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);

                    // Clean up shader resources
                    SDL_ReleaseGPUShader(gpu_device, vs);
                    SDL_ReleaseGPUShader(gpu_device, fs);
                }

                ~GridRenderer()
                {
                    if (vertex_buffer)
                    {
                        SDL_ReleaseGPUBuffer(gpu_device, vertex_buffer);
                    }
                    if (grid_pipeline)
                    {
                        SDL_ReleaseGPUGraphicsPipeline(gpu_device, grid_pipeline);
                    }
                }

                void cpu_update()
                {
                    // Check if any subdivision parameters have changed
                    bool needs_update = false;

                    uint32_t current_x_subdivisions = parameter_store.get<uint32_t>("visualizer.grid.x_subdivisions");
                    uint32_t current_y_subdivisions = parameter_store.get<uint32_t>("visualizer.grid.y_subdivisions");
                    uint32_t current_z_subdivisions = parameter_store.get<uint32_t>("visualizer.grid.z_subdivisions");

                    if (current_x_subdivisions != x_subdivisions || current_y_subdivisions != y_subdivisions ||
                        current_z_subdivisions != z_subdivisions)
                    {
                        x_subdivisions = current_x_subdivisions;
                        y_subdivisions = current_y_subdivisions;
                        z_subdivisions = current_z_subdivisions;
                        needs_update = true;
                    }

                    if (needs_update)
                    {
                        generate_grid_lines();
                    }
                }

                void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
                {
                    // Check if grid lines need to be updated
                    bool needs_update = false;

                    uint32_t current_x_subdivisions = parameter_store.get<uint32_t>("visualizer.grid.x_subdivisions");
                    uint32_t current_y_subdivisions = parameter_store.get<uint32_t>("visualizer.grid.y_subdivisions");
                    uint32_t current_z_subdivisions = parameter_store.get<uint32_t>("visualizer.grid.z_subdivisions");

                    if (current_x_subdivisions != x_subdivisions || current_y_subdivisions != y_subdivisions ||
                        current_z_subdivisions != z_subdivisions)
                    {
                        x_subdivisions = current_x_subdivisions;
                        y_subdivisions = current_y_subdivisions;
                        z_subdivisions = current_z_subdivisions;
                        generate_grid_lines();
                        needs_update = true;
                    }

                    if (needs_update && vertex_buffer)
                    {
                        // Recreate vertex buffer with new size if needed
                        SDL_ReleaseGPUBuffer(gpu_device, vertex_buffer);

                        SDL_GPUBufferCreateInfo vertex_buffer_create_info = {0};
                        vertex_buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
                        vertex_buffer_create_info.size = lines.size() * sizeof(glm::vec3);
                        vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &vertex_buffer_create_info);

                        // Upload updated grid lines data to GPU
                        upload_buffer->upload_to_gpu(copy_pass, vertex_buffer, lines.data(),
                                                     lines.size() * sizeof(glm::vec3));
                    }
                }

                void render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                 const glm::mat4 &mvp)
                {
                    if (!grid_pipeline || !vertex_buffer)
                        return;

                    // Bind the graphics pipeline
                    SDL_BindGPUGraphicsPipeline(render_pass, grid_pipeline);

                    // Bind the vertex buffer
                    SDL_BindGPUVertexBuffers(render_pass, 0, (SDL_GPUBufferBinding[]){vertex_buffer, 0}, 1);

                    // Push the MVP matrix uniform data
                    SDL_PushGPUVertexUniformData(command_buffer, 0, &mvp[0][0], sizeof(mvp));

                    // Draw the grid lines
                    SDL_DrawGPUPrimitives(render_pass, lines.size(), 1, 0, 0);
                }
        };

        class PointsRenderer
        {
            private:
                std::vector<glm::vec4> points;
                bool data_changed = false;
                size_t last_data_size = 0;

                ParameterStore &parameter_store;
                EventData &event_data;
                SDL_GPUDevice *gpu_device = nullptr;
                SDL_GPUGraphicsPipeline *points_pipeline = nullptr;
                SDL_GPUBuffer *vertex_buffer = nullptr;

                void generate_points_from_event_data()
                {
                    points.clear();
                    
                    // Lock the event data to safely access it
                    event_data.lock_data_vectors();
                    
                    // Get the event data vector (using relative timestamps)
                    const std::vector<glm::vec4> &evt_vector = event_data.get_evt_vector_ref(true);
                    
                    if (evt_vector.empty())
                    {
                        event_data.unlock_data_vectors();
                        return;
                    }
                    
                    // Find the bounding box of the event data for normalization
                    float min_x = evt_vector[0].x, max_x = evt_vector[0].x;
                    float min_y = evt_vector[0].y, max_y = evt_vector[0].y;
                    float min_t = evt_vector[0].z, max_t = evt_vector[0].z;
                    
                    for (const auto &evt : evt_vector)
                    {
                        min_x = std::min(min_x, evt.x);
                        max_x = std::max(max_x, evt.x);
                        min_y = std::min(min_y, evt.y);
                        max_y = std::max(max_y, evt.y);
                        min_t = std::min(min_t, evt.z);
                        max_t = std::max(max_t, evt.z);
                    }
                    
                    // Calculate normalization factors to map to [-1, 1] cube
                    float range_x = max_x - min_x;
                    float range_y = max_y - min_y;
                    float range_t = max_t - min_t;
                    
                    // Avoid division by zero
                    if (range_x == 0.0f) range_x = 1.0f;
                    if (range_y == 0.0f) range_y = 1.0f;
                    if (range_t == 0.0f) range_t = 1.0f;
                    
                    // Generate normalized points
                    for (const auto &evt : evt_vector)
                    {
                        // Normalize x, y coordinates to [-1, 1]
                        float norm_x = 2.0f * (evt.x - min_x) / range_x - 1.0f;
                        float norm_y = 2.0f * (evt.y - min_y) / range_y - 1.0f;
                        float norm_t = 2.0f * (evt.z - min_t) / range_t - 1.0f;
                        
                        // Store as vec4: (x, y, t, polarity)
                        points.push_back(glm::vec4(norm_x, norm_y, norm_t, evt.w));
                    }
                    
                    event_data.unlock_data_vectors();
                }

            public:
                PointsRenderer(ParameterStore &parameter_store, EventData &event_data, SDL_GPUDevice *gpu_device, 
                              UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
                    : parameter_store(parameter_store), event_data(event_data), gpu_device(gpu_device)
                {
                    generate_points_from_event_data();

                    SDL_GPUShaderCreateInfo vs_create_info = {0};
                    vs_create_info.code_size = sizeof points_vert;
                    vs_create_info.code = (const unsigned char *)points_vert;
                    vs_create_info.entrypoint = "main";
                    vs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
                    vs_create_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
                    vs_create_info.num_samplers = 0;
                    vs_create_info.num_storage_textures = 0;
                    vs_create_info.num_storage_buffers = 0;
                    vs_create_info.num_uniform_buffers = 1;
                    SDL_GPUShader *vs = SDL_CreateGPUShader(gpu_device, &vs_create_info);

                    SDL_GPUShaderCreateInfo fs_create_info = {0};
                    fs_create_info.code_size = sizeof points_frag;
                    fs_create_info.code = (const unsigned char *)points_frag;
                    fs_create_info.entrypoint = "main";
                    fs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
                    fs_create_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
                    fs_create_info.num_samplers = 0;
                    fs_create_info.num_storage_textures = 0;
                    fs_create_info.num_storage_buffers = 0;
                    fs_create_info.num_uniform_buffers = 0;
                    SDL_GPUShader *fs = SDL_CreateGPUShader(gpu_device, &fs_create_info);

                    // Create vertex buffer for points
                    SDL_GPUBufferCreateInfo vertex_buffer_create_info = {0};
                    vertex_buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
                    vertex_buffer_create_info.size = points.size() * sizeof(glm::vec4);
                    vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &vertex_buffer_create_info);

                    // Upload points data to GPU
                    upload_buffer->upload_to_gpu(copy_pass, vertex_buffer, points.data(),
                                                 points.size() * sizeof(glm::vec4));

                    // Create graphics pipeline for point rendering
                    SDL_GPUVertexBufferDescription vertex_buffer_desc = {0, sizeof(glm::vec4),
                                                                         SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
                    SDL_GPUVertexAttribute vertex_attribute = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0};
                    SDL_GPUColorTargetDescription color_target_desc = {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM};

                    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
                        .vertex_shader = vs,
                        .fragment_shader = fs,
                        .vertex_input_state = {.vertex_buffer_descriptions = &vertex_buffer_desc,
                                               .num_vertex_buffers = 1,
                                               .vertex_attributes = &vertex_attribute,
                                               .num_vertex_attributes = 1},
                        .primitive_type = SDL_GPU_PRIMITIVETYPE_POINTLIST,
                        .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                                                .enable_depth_test = true,
                                                .enable_depth_write = true},
                        .target_info = {.color_target_descriptions = &color_target_desc,
                                        .num_color_targets = 1,
                                        .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                                        .has_depth_stencil_target = true}};

                    points_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);

                    // Clean up shader resources
                    SDL_ReleaseGPUShader(gpu_device, vs);
                    SDL_ReleaseGPUShader(gpu_device, fs);
                }

                ~PointsRenderer()
                {
                    if (vertex_buffer)
                    {
                        SDL_ReleaseGPUBuffer(gpu_device, vertex_buffer);
                    }
                    if (points_pipeline)
                    {
                        SDL_ReleaseGPUGraphicsPipeline(gpu_device, points_pipeline);
                    }
                }

                void cpu_update()
                {
                    // Check if event data has changed
                    event_data.lock_data_vectors();
                    const std::vector<glm::vec4> &evt_vector = event_data.get_evt_vector_ref(true);
                    size_t current_data_size = evt_vector.size();
                    event_data.unlock_data_vectors();
                    
                    if (current_data_size != last_data_size)
                    {
                        data_changed = true;
                        last_data_size = current_data_size;
                    }
                }

                void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
                {
                    if (data_changed)
                    {
                        generate_points_from_event_data();
                        
                        if (vertex_buffer)
                        {
                            // Recreate vertex buffer with new size if needed
                            SDL_ReleaseGPUBuffer(gpu_device, vertex_buffer);
                        }

                        SDL_GPUBufferCreateInfo vertex_buffer_create_info = {0};
                        vertex_buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
                        vertex_buffer_create_info.size = points.size() * sizeof(glm::vec4);
                        vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &vertex_buffer_create_info);

                        // Upload updated points data to GPU
                        upload_buffer->upload_to_gpu(copy_pass, vertex_buffer, points.data(),
                                                     points.size() * sizeof(glm::vec4));
                        
                        data_changed = false;
                    }
                }

                void render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                const glm::mat4 &mvp)
                {
                    if (!points_pipeline || !vertex_buffer || points.empty())
                        return;

                    // Bind the graphics pipeline
                    SDL_BindGPUGraphicsPipeline(render_pass, points_pipeline);

                    // Bind the vertex buffer
                    SDL_BindGPUVertexBuffers(render_pass, 0, (SDL_GPUBufferBinding[]){vertex_buffer, 0}, 1);

                    // Create uniform buffer data for points shader
                    struct PointsUniforms {
                        glm::mat4 mvp;
                        glm::vec4 negative_color;
                        glm::vec4 positive_color;
                        float point_size;
                    } uniforms;
                    
                    uniforms.mvp = mvp;
                    uniforms.negative_color = glm::vec4(parameter_store.get<glm::vec3>("polarity_neg_color"), 1.0f);
                    uniforms.positive_color = glm::vec4(parameter_store.get<glm::vec3>("polarity_pos_color"), 1.0f);
                    uniforms.point_size = parameter_store.get<float>("particle_scale");

                    // Push the uniform data
                    SDL_PushGPUVertexUniformData(command_buffer, 0, &uniforms, sizeof(uniforms));

                    // Draw the points
                    SDL_DrawGPUPrimitives(render_pass, points.size(), 1, 0, 0);
                }
        };

        Camera camera;
        glm::vec3 box_min;
        glm::vec3 box_max;

        ParameterStore &parameter_store;
        std::unordered_map<std::string, RenderTarget> &render_targets;
        EventData &event_data;

        SDL_Window *window = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;

        GridRenderer *grid_renderer = nullptr;
        PointsRenderer *points_renderer = nullptr;
        SDL_GPUGraphicsPipeline *frame_pipeline = nullptr;
        SDL_GPUGraphicsPipeline *text_pipeline = nullptr;

        // Mouse state for camera orbiting
        bool is_mouse_dragging = false;
        float last_mouse_x = 0.0f;
        float last_mouse_y = 0.0f;
        bool cursor_captured = false;

    public:
        Visualizer(ParameterStore &parameter_store, std::unordered_map<std::string, RenderTarget> &render_targets,
                   EventData &event_data, SDL_Window *window, SDL_GPUDevice *gpu_device, UploadBuffer *upload_buffer,
                   SDL_GPUCopyPass *copy_pass)
            : parameter_store(parameter_store), render_targets(render_targets), event_data(event_data), window(window),
              gpu_device(gpu_device)
        {
            SDL_GPUTextureCreateInfo color_create_info = {
                .type = SDL_GPU_TEXTURETYPE_2D,
                .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
                .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
                .width = 1920,
                .height = 1200,
                .layer_count_or_depth = 1,
                .num_levels = 1,
                .sample_count = SDL_GPU_SAMPLECOUNT_1,
            };
            render_targets["VisualizerColor"] = {SDL_CreateGPUTexture(gpu_device, &color_create_info),
                                                 color_create_info.width, color_create_info.height};

            SDL_GPUTextureCreateInfo depth_create_info = {
                .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
                .width = 1920,
                .height = 1200,
                .layer_count_or_depth = 1,
                .num_levels = 1,
                .sample_count = SDL_GPU_SAMPLECOUNT_1,
            };
            render_targets["VisualizerDepth"] = {SDL_CreateGPUTexture(gpu_device, &depth_create_info),
                                                 depth_create_info.width, depth_create_info.height};
            
            camera = Camera(glm::vec3(0.0f, 0.0f, 0.0f), 4.0f, 45.0f, 1920.0f / 1200.0f, 0.1f, 1000.0f);

            grid_renderer = new GridRenderer(parameter_store, gpu_device, upload_buffer, copy_pass);
            points_renderer = new PointsRenderer(parameter_store, event_data, gpu_device, upload_buffer, copy_pass);
        }

        ~Visualizer()
        {
            if (grid_renderer)
            {
                delete grid_renderer;
            }
            if (points_renderer)
            {
                delete points_renderer;
            }

            SDL_ReleaseGPUTexture(gpu_device, render_targets["VisualizerDepth"].texture);
            SDL_ReleaseGPUTexture(gpu_device, render_targets["VisualizerColor"].texture);
        }

        bool event_handler(SDL_Event *event)
        {
            if (render_targets["VisualizerColor"].is_focused == true)
            {
                switch (event->type)
                {
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (event->button.button == SDL_BUTTON_LEFT)
                    {
                        is_mouse_dragging = true;
                        last_mouse_x = event->button.x;
                        last_mouse_y = event->button.y;

                        // Hide cursor and enable relative mouse mode for unlimited movement
                        SDL_HideCursor();
                        SDL_SetWindowRelativeMouseMode(window, true);
                        cursor_captured = true;
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (event->button.button == SDL_BUTTON_LEFT)
                    {
                        is_mouse_dragging = false;

                        // Restore cursor and disable relative mouse mode
                        if (cursor_captured)
                        {
                            SDL_SetWindowRelativeMouseMode(window, false);
                            SDL_ShowCursor();
                            cursor_captured = false;
                        }
                    }
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    if (is_mouse_dragging && cursor_captured)
                    {
                        // When relative mouse mode is enabled, motion.x and motion.y contain
                        // the relative movement from the last motion event
                        float x_offset = event->motion.xrel;
                        float y_offset = event->motion.yrel;

                        // Update camera rotation based on mouse movement
                        camera.processMouseMovement(x_offset, y_offset);
                    }
                    break;

                case SDL_EVENT_WINDOW_FOCUS_LOST:
                    // If cursor is captured and window loses focus, restore it
                    if (cursor_captured)
                    {
                        SDL_SetWindowRelativeMouseMode(window, false);
                        SDL_ShowCursor();
                        cursor_captured = false;
                        is_mouse_dragging = false;
                    }
                    break;

                default:
                    break;
                }
                render_targets["VisualizerColor"].is_focused = false;
                return true;
            }
            return false;
        }

        void cpu_update()
        {
            grid_renderer->cpu_update();
            points_renderer->cpu_update();
        }

        void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        {
            grid_renderer->copy_pass(upload_buffer, copy_pass);
            points_renderer->copy_pass(upload_buffer, copy_pass);
        }

        void compute_pass(SDL_GPUCommandBuffer *command_buffer)
        {
        }

        void render_pass(SDL_GPUCommandBuffer *command_buffer)
        {
            // create the color target info, this is the texture that will store the color data
            SDL_GPUColorTargetInfo color_target_info = {0};
            color_target_info.texture = render_targets["VisualizerColor"].texture;
            color_target_info.clear_color = (SDL_FColor){1.0f, 1.0f, 1.0f, 1.0f};
            color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
            color_target_info.store_op = SDL_GPU_STOREOP_STORE;
            color_target_info.mip_level = 0;
            color_target_info.layer_or_depth_plane = 0;
            color_target_info.cycle = false;

            // create the depth target info, this is the texture that will store the depth data
            SDL_GPUDepthStencilTargetInfo depth_target_info = {0};
            depth_target_info.texture = render_targets["VisualizerDepth"].texture;
            depth_target_info.clear_depth = 1.0f;
            depth_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
            depth_target_info.store_op = SDL_GPU_STOREOP_DONT_CARE;
            depth_target_info.cycle = false;

            // create the render pass, this is the pass that will be used to render the vertex data
            SDL_GPURenderPass *render_pass =
                SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, &depth_target_info);

            // Calculate MVP matrix for the grid
            glm::mat4 model = glm::mat4(1.0f); // Identity matrix for now
            glm::mat4 view = camera.getViewMatrix();
            glm::mat4 projection = camera.getProjectionMatrix();
            glm::mat4 mvp = projection * view * model;

            // Render the grid
            grid_renderer->render_pass(command_buffer, render_pass, mvp);

            // Render the points
            points_renderer->render_pass(command_buffer, render_pass, mvp);

            SDL_EndGPURenderPass(render_pass);
        }
};

#endif
