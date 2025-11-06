#pragma once

#ifndef VISUALIZER_HH
#define VISUALIZER_HH

#include "pch.hh"

#include "Camera.hh"
#include "EventData.hh"
#include "ParameterStore.hh"
#include "RenderTarget.hh"
#include "Scrubber.hh"
#include "UploadBuffer.hh"

#include "shaders/visualizer/frames/frames_frag.h"
#include "shaders/visualizer/frames/frames_vert.h"
#include "shaders/visualizer/grid/grid_frag.h"
#include "shaders/visualizer/grid/grid_vert.h"
#include "shaders/visualizer/points/points_frag.h"
#include "shaders/visualizer/points/points_vert.h"
#include "shaders/visualizer/text/text_frag.h"
#include "shaders/visualizer/text/text_vert.h"

#include "fonts/CascadiaCode.ttf.h"

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
                    // 1. Front face (Z = +1.0f)
                    // 2. Bottom face (Y = -1.0f)
                    // 3. Left face (X = -1.0f)

                    // Front face (Z = +1.0f) - lines parallel to X and Y axes
                    for (uint32_t i = 0; i <= x_subdivisions; ++i)
                    {
                        float x = 2.0f * static_cast<float>(i) / static_cast<float>(x_subdivisions) - 1.0f;
                        // Lines parallel to Y-axis on front face
                        lines.push_back(glm::vec3(x, -1.0f, 1.0f));
                        lines.push_back(glm::vec3(x, 1.0f, 1.0f));
                    }
                    for (uint32_t i = 0; i <= y_subdivisions; ++i)
                    {
                        float y = 2.0f * static_cast<float>(i) / static_cast<float>(y_subdivisions) - 1.0f;
                        // Lines parallel to X-axis on front face
                        lines.push_back(glm::vec3(-1.0f, y, 1.0f));
                        lines.push_back(glm::vec3(1.0f, y, 1.0f));
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
                                 const glm::mat4 &vp)
                {
                    if (!grid_pipeline || !vertex_buffer)
                        return;

                    // Bind the graphics pipeline
                    SDL_BindGPUGraphicsPipeline(render_pass, grid_pipeline);

                    // Bind the vertex buffer
                    SDL_GPUBufferBinding vertex_buffer_binding[] = {vertex_buffer, 0};
                    SDL_BindGPUVertexBuffers(render_pass, 0, vertex_buffer_binding, 1);

                    // Push the MVP matrix uniform data
                    SDL_PushGPUVertexUniformData(command_buffer, 0, &vp[0][0], sizeof(vp));

                    // Draw the grid lines
                    SDL_DrawGPUPrimitives(render_pass, lines.size(), 1, 0, 0);
                }
        };

        class PointsRenderer
        {
            private:
                ParameterStore &parameter_store;
                Scrubber *scrubber = nullptr;
                EventData &event_data;
                SDL_GPUDevice *gpu_device = nullptr;
                SDL_GPUGraphicsPipeline *points_pipeline = nullptr;

            public:
                PointsRenderer(ParameterStore &parameter_store, EventData &event_data, Scrubber *scrubber,
                               SDL_GPUDevice *gpu_device, UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
                    : parameter_store(parameter_store), event_data(event_data), scrubber(scrubber),
                      gpu_device(gpu_device)
                {

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
                    if (points_pipeline)
                    {
                        SDL_ReleaseGPUGraphicsPipeline(gpu_device, points_pipeline);
                    }
                }

                void cpu_update()
                {
                }

                void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
                {
                }

                void render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                 const glm::mat4 &vp)
                {

                    if (scrubber->get_points_buffer_size() == 0)
                        return;

                    // Bind the graphics pipeline
                    SDL_BindGPUGraphicsPipeline(render_pass, points_pipeline);

                    // Bind the vertex buffer
                    SDL_GPUBufferBinding vertex_buffer_binding[] = {scrubber->get_points_buffer(), 0};
                    SDL_BindGPUVertexBuffers(render_pass, 0, vertex_buffer_binding, 1);

                    // Create uniform buffer data for points shader
                    struct PointsUniforms
                    {
                            glm::mat4 mvp;
                            glm::vec4 negative_color;
                            glm::vec4 positive_color;
                            float point_size;
                    } uniforms;

                    glm::vec2 camera_resolution = scrubber->get_camera_resolution();
                    float lower_depth = scrubber->get_lower_depth();
                    float upper_depth = scrubber->get_upper_depth();
                    float depth_range = upper_depth - lower_depth;

                    // Initialize as an identity matrix
                    glm::mat4 z_translate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -lower_depth));
                    glm::mat4 scale_matrix =
                        glm::scale(glm::mat4(1.0f), glm::vec3(2.0f / camera_resolution.x, 2.0f / camera_resolution.y,
                                                              2.0f / depth_range));
                    glm::mat4 translate_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, -1.0f, -1.0f));
                    glm::mat4 rotate_matrix =
                        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                    glm::mat4 z_switch =
                        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

                    uniforms.mvp = vp * z_switch * rotate_matrix * translate_matrix * scale_matrix * z_translate;

                    // Get camera dimensions for scaling
                    uniforms.negative_color = glm::vec4(parameter_store.get<glm::vec3>("polarity_neg_color"), 1.0f);
                    uniforms.positive_color = glm::vec4(parameter_store.get<glm::vec3>("polarity_pos_color"), 1.0f);
                    uniforms.point_size = parameter_store.get<float>("particle_scale");

                    // Push the uniform data
                    SDL_PushGPUVertexUniformData(command_buffer, 0, &uniforms, sizeof(uniforms));

                    // Draw the points
                    SDL_DrawGPUPrimitives(render_pass, scrubber->get_points_buffer_size(), 1, 0, 0);
                }
        };

        // very ai generated
        class TextRenderer
        {
            private:
                // Vertex definition, based on the example
                struct TextVertex
                {
                        glm::vec3 pos;
                        SDL_FColor colour;
                        glm::vec2 uv;
                };

                // Stores info for a single draw call (one per atlas texture)
                struct TextDrawCall
                {
                        SDL_GPUTexture *atlas_texture;
                        Uint32 index_count;
                        Uint32 index_offset;
                        Sint32 base_vertex;
                };

                ParameterStore &parameter_store;
                SDL_GPUDevice *gpu_device = nullptr;
                SDL_GPUGraphicsPipeline *text_pipeline = nullptr;
                TTF_TextEngine *text_engine = nullptr;
                TTF_Font *font = nullptr;
                SDL_GPUSampler *sampler = nullptr;

                SDL_GPUBuffer *vertex_buffer = nullptr;
                SDL_GPUBuffer *index_buffer = nullptr;

                // CPU-side buffers for geometry
                std::vector<TextVertex> vertices;
                std::vector<Uint32> indices;

                // List of draw calls to make
                std::vector<TextDrawCall> draw_calls;

                // List of text objects to be freed in cpu_update
                std::vector<TTF_Text *> managed_text_objects;

            public:
                TextRenderer(ParameterStore &parameter_store, SDL_GPUDevice *gpu_device)
                    : parameter_store(parameter_store), gpu_device(gpu_device)
                {
                    TTF_Init();
                    text_engine = TTF_CreateGPUTextEngine(gpu_device);
                    SDL_IOStream *io = SDL_IOFromConstMem(CascadiaCode_ttf, sizeof CascadiaCode_ttf);
                    font = TTF_OpenFontIO(io, true, 24.0f);

                    // --- Create Shaders (You must provide these files) ---
                    SDL_GPUShaderCreateInfo vs_create_info = {0};
                    vs_create_info.code_size = sizeof text_vert;
                    vs_create_info.code = (const unsigned char *)text_vert;
                    vs_create_info.entrypoint = "main";
                    vs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
                    vs_create_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
                    vs_create_info.num_uniform_buffers = 1; // For VP matrix
                    SDL_GPUShader *vs = SDL_CreateGPUShader(gpu_device, &vs_create_info);

                    SDL_GPUShaderCreateInfo fs_create_info = {0};
                    fs_create_info.code_size = sizeof text_frag;
                    fs_create_info.code = (const unsigned char *)text_frag;
                    fs_create_info.entrypoint = "main";
                    fs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
                    fs_create_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
                    fs_create_info.num_samplers = 1; // For atlas texture
                    SDL_GPUShader *fs = SDL_CreateGPUShader(gpu_device, &fs_create_info);

                    // --- Create Pipeline (Based on example and GridRenderer) ---
                    SDL_GPUVertexAttribute vertex_attributes[] = {
                        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(TextVertex, pos)},
                        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(TextVertex, colour)},
                        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(TextVertex, uv)}};
                    SDL_GPUVertexBufferDescription vertex_buffer_desc = {0, sizeof(TextVertex),
                                                                         SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
                    SDL_GPUColorTargetDescription color_target_desc = {
                        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,                         // Match GridRenderer
                        .blend_state = {.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA, // From example code
                                        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                        .color_blend_op = SDL_GPU_BLENDOP_ADD,
                                        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                                        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_DST_ALPHA,
                                        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                                        .color_write_mask = 0xF,
                                        .enable_blend = true}};

                    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
                        .vertex_shader = vs,
                        .fragment_shader = fs,
                        .vertex_input_state = {.vertex_buffer_descriptions = &vertex_buffer_desc,
                                               .num_vertex_buffers = 1,
                                               .vertex_attributes = vertex_attributes,
                                               .num_vertex_attributes = 3},
                        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
                        .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL, // From GridRenderer
                                                .enable_depth_test = true,
                                                .enable_depth_write = true},
                        .target_info = {.color_target_descriptions = &color_target_desc,
                                        .num_color_targets = 1,
                                        .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM, // From GridRenderer
                                        .has_depth_stencil_target = true}};
                    text_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);

                    SDL_ReleaseGPUShader(gpu_device, vs);
                    SDL_ReleaseGPUShader(gpu_device, fs);

                    // --- Create Sampler (From example code) ---
                    SDL_GPUSamplerCreateInfo sampler_info = {.min_filter = SDL_GPU_FILTER_LINEAR,
                                                             .mag_filter = SDL_GPU_FILTER_LINEAR,
                                                             .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
                                                             .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                             .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                             .address_mode_w =
                                                                 SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE};
                    sampler = SDL_CreateGPUSampler(gpu_device, &sampler_info);
                }

                ~TextRenderer()
                {
                    // Clear any remaining text objects
                    cpu_update();

                    if (sampler)
                        SDL_ReleaseGPUSampler(gpu_device, sampler);
                    if (index_buffer)
                        SDL_ReleaseGPUBuffer(gpu_device, index_buffer);
                    if (vertex_buffer)
                        SDL_ReleaseGPUBuffer(gpu_device, vertex_buffer);
                    if (text_pipeline)
                        SDL_ReleaseGPUGraphicsPipeline(gpu_device, text_pipeline);
                    if (font)
                        TTF_CloseFont(font);
                    if (text_engine)
                        TTF_DestroyGPUTextEngine(text_engine);
                }

                /**
                 * @brief Queues text to be rendered in 3D space. Call this after cpu_update() and before copy_pass().
                 * @param text The string to render.
                 * @param position The 3D world-space position (translation) for the text.
                 * @param normal (Currently unused) The normal direction for orientation.
                 * @param color The color of the text.
                 */
                void add_text(const std::string &text, const glm::vec3 &position, const glm::vec3 &normal,
                              const SDL_FColor &color)
                {
                    TTF_Text *text_obj = TTF_CreateText(text_engine, font, text.c_str(), 0);
                    managed_text_objects.push_back(text_obj); // Add to list for cleanup

                    TTF_GPUAtlasDrawSequence *sequence = TTF_GetGPUTextDrawData(text_obj);
                    if (!sequence)
                        return; // Nothing to draw

                    // Scale factor to convert from pixel space to world space
                    // SDL_ttf coordinates are in pixels, so we need to scale them down for 3D space
                    // Using a small scale factor so text appears as a reasonable size in 3D
                    const float pixel_to_world_scale = 0.0025f; // 1000 pixels = 1 world unit

                    // Build rotation matrix from normal to orient the text plane
                    // The text plane is in XY, so the normal is Z
                    glm::vec3 normal_norm = glm::normalize(normal);

                    // Build an orthonormal basis where:
                    // - Z axis is the normal (facing direction)
                    // - X and Y axes span the plane
                    glm::vec3 forward = normal_norm;

                    // Choose a reference vector - prefer Y-up if normal is not parallel to it
                    glm::vec3 up_ref = glm::vec3(0.0f, 1.0f, 0.0f);
                    if (glm::abs(glm::dot(forward, up_ref)) > 0.99f)
                    {
                        // Normal is nearly parallel to up, use Z as reference instead
                        up_ref = glm::vec3(0.0f, 0.0f, 1.0f);
                    }

                    // Use Gram-Schmidt to build orthonormal basis
                    glm::vec3 right = glm::normalize(glm::cross(forward, up_ref));
                    glm::vec3 up = glm::normalize(glm::cross(right, forward));

                    // Build rotation matrix from the basis vectors
                    // Column-major order: right, up, forward (negative because camera looks along -Z)
                    glm::mat4 rotation_matrix =
                        glm::mat4(right.x, up.x, -forward.x, 0.0f, right.y, up.y, -forward.y, 0.0f, right.z, up.z,
                                  -forward.z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

                    // Scale first, then rotate, then translate
                    glm::mat4 model_matrix = glm::translate(glm::mat4(1.0f), position) * rotation_matrix;

                    // Iterate through the sequence linked list (for multiple atlases)
                    for (TTF_GPUAtlasDrawSequence *seq = sequence; seq != NULL; seq = seq->next)
                    {
                        Uint32 current_index_offset = static_cast<Uint32>(indices.size());
                        Sint32 current_vertex_offset = static_cast<Sint32>(vertices.size());

                        // Add vertices for this sequence
                        for (int i = 0; i < seq->num_vertices; i++)
                        {
                            const SDL_FPoint pos2D = seq->xy[i];
                            // Transform 2D quad vertex into 3D world space
                            // Scale pixel coordinates to world space
                            // Using -pos2D.y to flip Y-axis (SDL_ttf is Y-down, 3D space is Y-up)
                            glm::vec4 pos3D_world =
                                model_matrix *
                                glm::vec4(-pos2D.x * pixel_to_world_scale, pos2D.y * pixel_to_world_scale, 0.0f, 1.0f);

                            TextVertex vert;
                            vert.pos = glm::vec3(pos3D_world);
                            vert.colour = color;
                            vert.uv = glm::vec2(seq->uv[i].x, seq->uv[i].y);
                            vertices.push_back(vert);
                        }

                        // Add indices for this sequence
                        for (int i = 0; i < seq->num_indices; i++)
                        {
                            // Add the base vertex offset to the local index
                            indices.push_back(current_vertex_offset + seq->indices[i]);
                        }

                        // Add a draw call for this atlas texture
                        draw_calls.push_back({.atlas_texture = seq->atlas_texture,
                                              .index_count = static_cast<Uint32>(seq->num_indices),
                                              .index_offset = current_index_offset,
                                              .base_vertex = current_vertex_offset});
                    }
                }

                // Call this once per frame *before* queueing new text
                void cpu_update()
                {
                    // Clear data from previous frame
                    vertices.clear();
                    indices.clear();
                    draw_calls.clear();

                    // Free all TTF_Text objects
                    for (TTF_Text *text_obj : managed_text_objects)
                    {
                        TTF_DestroyText(text_obj);
                    }
                    managed_text_objects.clear();
                }

                void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
                {
                    if (vertices.empty() || indices.empty())
                        return;

                    // Resize GPU buffers if needed
                    size_t vertex_buffer_size = vertices.size() * sizeof(TextVertex);

                    if (vertex_buffer)
                        SDL_ReleaseGPUBuffer(gpu_device, vertex_buffer);
                    SDL_GPUBufferCreateInfo vbf_info = {.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                                                        .size = static_cast<Uint32>(vertex_buffer_size)};
                    vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &vbf_info);

                    size_t index_buffer_size = indices.size() * sizeof(Uint32);

                    if (index_buffer)
                        SDL_ReleaseGPUBuffer(gpu_device, index_buffer);
                    SDL_GPUBufferCreateInfo ibf_info = {.usage = SDL_GPU_BUFFERUSAGE_INDEX,
                                                        .size = static_cast<Uint32>(index_buffer_size)};
                    index_buffer = SDL_CreateGPUBuffer(gpu_device, &ibf_info);

                    // Upload data
                    upload_buffer->upload_to_gpu(copy_pass, vertex_buffer, vertices.data(), vertex_buffer_size);
                    upload_buffer->upload_to_gpu(copy_pass, index_buffer, indices.data(), index_buffer_size);
                }

                void render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                 const glm::mat4 &vp)
                {
                    if (draw_calls.empty() || !vertex_buffer || !index_buffer || !text_pipeline)
                        return;

                    // Bind pipeline and buffers once
                    SDL_BindGPUGraphicsPipeline(render_pass, text_pipeline);

                    // Iterate through the draw calls, binding atlases as needed
                    for (const auto &call : draw_calls)
                    {
                        // Push the View-Projection matrix
                        SDL_PushGPUVertexUniformData(command_buffer, 0, &vp[0][0], sizeof(vp));

                        SDL_GPUBufferBinding v_binding = {vertex_buffer, 0};
                        SDL_BindGPUVertexBuffers(render_pass, 0, &v_binding, 1);

                        SDL_GPUBufferBinding i_binding = {index_buffer, 0};
                        SDL_BindGPUIndexBuffer(render_pass, &i_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

                        SDL_GPUTextureSamplerBinding sampler_binding = {.texture = call.atlas_texture,
                                                                        .sampler = sampler};
                        SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);

                        // Draw the primitives for this batch
                        SDL_DrawGPUIndexedPrimitives(render_pass, call.index_count, 1, call.index_offset,
                                                     call.base_vertex, 0);
                    }
                }
        };

        class FramesRenderer
        {
            private:
                ParameterStore *parameter_store = nullptr;
                SDL_GPUDevice *gpu_device = nullptr;
                Scrubber *scrubber = nullptr;
                SDL_GPUGraphicsPipeline *frames_pipeline = nullptr;
                SDL_GPUSampler *sampler = nullptr;

            public:
                FramesRenderer(ParameterStore *parameter_store, SDL_GPUDevice *gpu_device, Scrubber *scrubber)
                    : parameter_store(parameter_store), gpu_device(gpu_device), scrubber(scrubber)
                {
                    SDL_GPUShaderCreateInfo vs_create_info = {0};
                    vs_create_info.code_size = sizeof frames_vert;
                    vs_create_info.code = (const unsigned char *)frames_vert;
                    vs_create_info.entrypoint = "main";
                    vs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
                    vs_create_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
                    vs_create_info.num_samplers = 0;
                    vs_create_info.num_storage_textures = 0;
                    vs_create_info.num_storage_buffers = 0;
                    vs_create_info.num_uniform_buffers = 1;
                    SDL_GPUShader *vs = SDL_CreateGPUShader(gpu_device, &vs_create_info);

                    SDL_GPUShaderCreateInfo fs_create_info = {0};
                    fs_create_info.code_size = sizeof frames_frag;
                    fs_create_info.code = (const unsigned char *)frames_frag;
                    fs_create_info.entrypoint = "main";
                    fs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
                    fs_create_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
                    fs_create_info.num_samplers = 1;
                    fs_create_info.num_storage_textures = 0;
                    fs_create_info.num_storage_buffers = 0;
                    fs_create_info.num_uniform_buffers = 1;
                    SDL_GPUShader *fs = SDL_CreateGPUShader(gpu_device, &fs_create_info);

                    SDL_GPUColorTargetDescription color_target_desc = {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM};
                    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
                        .vertex_shader = vs,
                        .fragment_shader = fs,
                        .vertex_input_state = {.vertex_buffer_descriptions = nullptr,
                                               .num_vertex_buffers = 0,
                                               .vertex_attributes = nullptr,
                                               .num_vertex_attributes = 0},
                        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
                        .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                                                .enable_depth_test = true,
                                                .enable_depth_write = true},
                        .target_info = {.color_target_descriptions = &color_target_desc,
                                        .num_color_targets = 1,
                                        .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                                        .has_depth_stencil_target = true}};

                    frames_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);

                    // Clean up shader resources
                    SDL_ReleaseGPUShader(gpu_device, vs);
                    SDL_ReleaseGPUShader(gpu_device, fs);

                    SDL_GPUSamplerCreateInfo sampler_info = {};
                    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
                    sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
                    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
                    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                    sampler_info.mip_lod_bias = 0.0f;
                    sampler_info.min_lod = -1000.0f;
                    sampler_info.max_lod = 1000.0f;
                    sampler_info.enable_anisotropy = false;
                    sampler_info.max_anisotropy = 1.0f;
                    sampler_info.enable_compare = false;
                    sampler = SDL_CreateGPUSampler(gpu_device, &sampler_info);
                }

                ~FramesRenderer()
                {
                    if (sampler)
                    {
                        SDL_ReleaseGPUSampler(gpu_device, sampler);
                    }
                    if (frames_pipeline)
                    {
                        SDL_ReleaseGPUGraphicsPipeline(gpu_device, frames_pipeline);
                    }
                }

                void cpu_update()
                {
                }

                void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
                {
                }

                void render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                 const glm::mat4 &vp)
                {
                    if (!frames_pipeline || scrubber->get_frames_timestamps()[0] < 0.0f)
                    {
                        return;
                    }

                    SDL_BindGPUGraphicsPipeline(render_pass, frames_pipeline);

                    glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                    glm::mat4 mvp = vp * rotate;
                    SDL_PushGPUVertexUniformData(command_buffer, 0, &mvp[0][0], sizeof(mvp));

                    SDL_GPUTextureSamplerBinding sampler_binding = {};
                    sampler_binding.texture = scrubber->get_frames_texture();
                    sampler_binding.sampler = sampler;
                    SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);

                    glm::vec4 frame_data = {scrubber->get_frames_timestamps()[0], scrubber->get_frames_timestamps()[1],
                                            scrubber->get_upper_depth(), 0.0f};
                    SDL_PushGPUFragmentUniformData(command_buffer, 0, &frame_data, sizeof(frame_data));

                    SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);
                }
        };

        Camera camera;
        glm::vec3 box_min;
        glm::vec3 box_max;

        ParameterStore &parameter_store;
        std::unordered_map<std::string, RenderTarget> &render_targets;
        EventData &event_data;
        Scrubber *scrubber = nullptr;

        SDL_Window *window = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;

        GridRenderer *grid_renderer = nullptr;
        PointsRenderer *points_renderer = nullptr;
        TextRenderer *text_renderer = nullptr;
        FramesRenderer *frames_renderer = nullptr;

        // Mouse state for camera orbiting
        bool is_mouse_dragging = false;
        float last_mouse_x = 0.0f;
        float last_mouse_y = 0.0f;
        bool cursor_captured = false;

    public:
        Visualizer(ParameterStore &parameter_store, std::unordered_map<std::string, RenderTarget> &render_targets,
                   EventData &event_data, Scrubber *scrubber, SDL_Window *window, SDL_GPUDevice *gpu_device,
                   UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
            : parameter_store(parameter_store), render_targets(render_targets), event_data(event_data),
              scrubber(scrubber), window(window), gpu_device(gpu_device)
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
            points_renderer =
                new PointsRenderer(parameter_store, event_data, scrubber, gpu_device, upload_buffer, copy_pass);
            text_renderer = new TextRenderer(parameter_store, gpu_device);
            frames_renderer = new FramesRenderer(&parameter_store, gpu_device, scrubber);
        }

        ~Visualizer()
        {
            if (frames_renderer)
            {
                delete frames_renderer;
            }
            if (grid_renderer)
            {
                delete grid_renderer;
            }
            if (points_renderer)
            {
                delete points_renderer;
            }
            if (text_renderer)
            {
                delete text_renderer;
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
                        float x_offset = -event->motion.xrel;
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
            text_renderer->cpu_update();
            frames_renderer->cpu_update();

            // Add timestamp labels for each z subdivision
            if (!scrubber)
                return;

            // Get z subdivisions from parameter store
            uint32_t z_subdivisions = parameter_store.get<uint32_t>("visualizer.grid.z_subdivisions");

            // Get depth range from scrubber
            float lower_depth = scrubber->get_lower_depth();
            float upper_depth = scrubber->get_upper_depth();
            float depth_range = upper_depth - lower_depth;

            // Position for labels: near the bottom of the grid (Y = -1.0f), slightly offset in X
            // Normal vector points towards the camera (along Z axis)
            glm::vec3 text_position = {1.0f, -1.0f, 0.0f};
            glm::vec3 text_normal = {1.0f, 0.0f, 0.0f};
            SDL_FColor text_color = {0.0f, 0.0f, 0.0f, 1.0f}; // White color

            // Add labels for each z subdivision
            for (uint32_t i = 0; i <= z_subdivisions; ++i)
            {
                // Calculate normalized Z position [-1, 1]
                float normalized_z = 2.0f * static_cast<float>(i) / static_cast<float>(z_subdivisions) - 1.0f;

                // Convert normalized Z to actual depth value
                float timestamp = lower_depth + (normalized_z + 1.0f) * 0.5f * depth_range;

                // Format timestamp as string with reasonable precision
                std::string timestamp_str = std::format("{:.2f}", timestamp);

                // Position text at the corresponding Z subdivision
                text_position.z = normalized_z;

                // Add the text
                text_renderer->add_text(timestamp_str, text_position, text_normal, text_color);
            }
        }

        void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        {
            grid_renderer->copy_pass(upload_buffer, copy_pass);
            points_renderer->copy_pass(upload_buffer, copy_pass);
            text_renderer->copy_pass(upload_buffer, copy_pass);
            frames_renderer->copy_pass(upload_buffer, copy_pass);
        }

        void compute_pass(SDL_GPUCommandBuffer *command_buffer)
        {
        }

        void render_pass(SDL_GPUCommandBuffer *command_buffer)
        {
            // auto ts = scrubber->get_frames_timestamps();
            // if (ts[0] > 0.0f)
            // {
            //     // debug blit the frame to the back to test it
            //     auto frame_texture = scrubber->get_frames_texture();
            //     auto dims = scrubber->get_frame_dimensions();
            //     SDL_GPUBlitInfo blit_info = {};
            //     blit_info.source = SDL_GPUBlitRegion{frame_texture, 0, 0, 0, 0, (uint32_t)dims[0],
            //     (uint32_t)dims[1]}; blit_info.destination = SDL_GPUBlitRegion{
            //         render_targets["VisualizerColor"].texture, 0, 0, 0, 0, render_targets["VisualizerColor"].width,
            //         render_targets["VisualizerColor"].height};
            //     blit_info.load_op = SDL_GPU_LOADOP_DONT_CARE;
            //     blit_info.clear_color = {};
            //     blit_info.flip_mode = SDL_FLIP_NONE;
            //     blit_info.filter = SDL_GPU_FILTER_NEAREST;
            //     blit_info.cycle = false;
            //     SDL_BlitGPUTexture(command_buffer, &blit_info);
            // }

            // create the color target info, this is the texture that will store the color data
            SDL_GPUColorTargetInfo color_target_info = {0};
            color_target_info.texture = render_targets["VisualizerColor"].texture;
            SDL_FColor color = {1.0f, 1.0f, 1.0f, 1.0f};
            color_target_info.clear_color = color;
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
            glm::mat4 view = camera.getViewMatrix();
            glm::mat4 projection = camera.getProjectionMatrix();
            glm::mat4 vp = projection * view;

            // Render the grid
            grid_renderer->render_pass(command_buffer, render_pass, vp);

            // Render the points
            points_renderer->render_pass(command_buffer, render_pass, vp);

            // Render the frames
            frames_renderer->render_pass(command_buffer, render_pass, vp);

            // Render the text
            text_renderer->render_pass(command_buffer, render_pass, vp);

            SDL_EndGPURenderPass(render_pass);
        }
};

#endif
