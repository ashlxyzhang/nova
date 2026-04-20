#pragma once
#ifndef VISUALIZER_HH
#define VISUALIZER_HH

#include "util/pch.hh"

#include "data/EventData.hh"
#include "render/Camera.hh"
#include "render/RenderTarget.hh"
#include "render/UploadBuffer.hh"
#include "render/GPUDevice.hh"
#include "ui/Scrubber.hh"
#include "util/ErrorQueue.hh"

#include "shaders/visualizer/frames/frames_frag.h"
#include "shaders/visualizer/frames/frames_vert.h"
#include "shaders/visualizer/grid/grid_frag.h"
#include "shaders/visualizer/grid/grid_vert.h"
#include "shaders/visualizer/points/points_frag.h"
#include "shaders/visualizer/points/points_vert.h"
#include "shaders/visualizer/text/text_frag.h"
#include "shaders/visualizer/text/text_vert.h"

#include "fonts/CascadiaCode.ttf.h"

#include <memory>
struct DataSource;

/**
 * @brief Provides functions for rendering the 3D event data particle plot visualization (3D Visualizer window).
 */
class Visualizer
{
    public:
        // Enum for easily identifying time codes, duplicated in GUI.hh
        enum class TIME : uint8_t
        {
            UNIT_S = 0,
            UNIT_MS = 1,
            UNIT_US = 2
        };

        struct RenderTargets
        {
            RenderTarget color;
            RenderTarget depth;

            void init_textures(SDL_GPUDevice* gpu_device, cv::Size resolution) {
                SDL_GPUTextureCreateInfo vis_color_create_info = {
                    .type = SDL_GPU_TEXTURETYPE_2D,
                    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
                    .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
                    .width = 1920,
                    .height = 1200,
                    .layer_count_or_depth = 1,
                    .num_levels = 1,
                    .sample_count = SDL_GPU_SAMPLECOUNT_1,
                };
                color = {SDL_CreateGPUTexture(gpu_device, &vis_color_create_info), vis_color_create_info.width, vis_color_create_info.height};
                
                SDL_GPUTextureCreateInfo vis_depth_create_info = {
                    .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                    .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
                    .width = 1920,
                    .height = 1200,
                    .layer_count_or_depth = 1,
                    .num_levels = 1,
                    .sample_count = SDL_GPU_SAMPLECOUNT_1,
                };
                depth = {SDL_CreateGPUTexture(gpu_device, &vis_depth_create_info), vis_depth_create_info.width, vis_depth_create_info.height};
            }

            void delete_textures(SDL_GPUDevice* gpu_device) {
                SDL_ReleaseGPUTexture(gpu_device, color.texture);
                SDL_ReleaseGPUTexture(gpu_device, depth.texture);
            }
        };

        struct Parameters
        {
            uint32_t grid_x_subdivisions = 5;
            uint32_t grid_y_subdivisions = 5;
            uint32_t grid_z_subdivisions = 5;

            float particle_scale = 3.0f;
            glm::vec3 polarity_neg_color = glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 polarity_pos_color = glm::vec3(0.0f, 1.0f, 0.0f);

            TIME unit_type = TIME::UNIT_MS;           // MS is default
            float unit_time_conversion_factor = 1.0f; // MS is default
        };

    private:
        /**
         * @brief Provides functions for rendering the grid in the visualizer.
         */
        class GridRenderer
        {
            private:
                SDL_GPUDevice *gpu_device = nullptr;
                SDL_GPUGraphicsPipeline *grid_pipeline = nullptr;
                SDL_GPUBuffer *vertex_buffer = nullptr;
                std::vector<glm::vec3> lines;

                /**
                 * @brief Generates grid lines in 3D Visualizer window.
                 */
                void generate_grid_lines(const Parameters &params)
                {
                    lines.clear();

                    // Generate lines only on the three outside faces:
                    // 1. Front face (Z = +1.0f)
                    // 2. Bottom face (Y = -1.0f)
                    // 3. Left face (X = -1.0f)

                    // Front face (Z = +1.0f) - lines parallel to X and Y axes
                    for (uint32_t i = 0; i <= params.grid_x_subdivisions; ++i)
                    {
                        float x = 2.0f * static_cast<float>(i) / static_cast<float>(params.grid_x_subdivisions) - 1.0f;
                        lines.push_back(glm::vec3(x, -1.0f, 1.0f));
                        lines.push_back(glm::vec3(x, 1.0f, 1.0f));
                    }
                    for (uint32_t i = 0; i <= params.grid_y_subdivisions; ++i)
                    {
                        float y = 2.0f * static_cast<float>(i) / static_cast<float>(params.grid_y_subdivisions) - 1.0f;
                        lines.push_back(glm::vec3(-1.0f, y, 1.0f));
                        lines.push_back(glm::vec3(1.0f, y, 1.0f));
                    }

                    // Bottom face (Y = -1.0f) - lines parallel to X and Z axes
                    for (uint32_t i = 0; i <= params.grid_x_subdivisions; ++i)
                    {
                        float x = 2.0f * static_cast<float>(i) / static_cast<float>(params.grid_x_subdivisions) - 1.0f;
                        lines.push_back(glm::vec3(x, -1.0f, -1.0f));
                        lines.push_back(glm::vec3(x, -1.0f, 1.0f));
                    }
                    for (uint32_t i = 0; i <= params.grid_z_subdivisions; ++i)
                    {
                        float z = 2.0f * static_cast<float>(i) / static_cast<float>(params.grid_z_subdivisions) - 1.0f;
                        lines.push_back(glm::vec3(-1.0f, -1.0f, z));
                        lines.push_back(glm::vec3(1.0f, -1.0f, z));
                    }

                    // Left face (X = -1.0f) - lines parallel to Y and Z axes
                    for (uint32_t i = 0; i <= params.grid_y_subdivisions; ++i)
                    {
                        float y = 2.0f * static_cast<float>(i) / static_cast<float>(params.grid_y_subdivisions) - 1.0f;
                        lines.push_back(glm::vec3(-1.0f, y, -1.0f));
                        lines.push_back(glm::vec3(-1.0f, y, 1.0f));
                    }
                    for (uint32_t i = 0; i <= params.grid_z_subdivisions; ++i)
                    {
                        float z = 2.0f * static_cast<float>(i) / static_cast<float>(params.grid_z_subdivisions) - 1.0f;
                        lines.push_back(glm::vec3(-1.0f, -1.0f, z));
                        lines.push_back(glm::vec3(-1.0f, 1.0f, z));
                    }
                }

            public:
                /**
                 * @brief Constructor. Initializes pipeline and buffers.
                 * @param gpu_device SDL_GPUDevice to create resources
                 */
                GridRenderer(SDL_GPUDevice *gpu_device) : gpu_device(gpu_device)
                {
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

                    SDL_ReleaseGPUShader(gpu_device, vs);
                    SDL_ReleaseGPUShader(gpu_device, fs);
                }

                /**
                 * @brief Destructor, releases necessary buffers and pipelines from GPU device.
                 */
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

                /**
                 * @brief Updates grid visualization on each frame.
                 */
                void cpu_update(const Parameters &params)
                {
                    generate_grid_lines(params);
                }

                /**
                 * @brief Uploads updated grid lines to GPU.
                 * @param upload_buffer UploadBuffer object for uploading data to GPU
                 * @param copy_pass SDL_GPU_CopyPass for copying data to GPU
                 */
                void copy_pass(UploadBuffer &upload_buffer, SDL_GPUCopyPass *copy_pass)
                {
                    if (lines.empty())
                        return;

                    // Recreate vertex buffer with current line data
                    if (vertex_buffer)
                    {
                        SDL_ReleaseGPUBuffer(gpu_device, vertex_buffer);
                    }

                    SDL_GPUBufferCreateInfo vertex_buffer_create_info = {0};
                    vertex_buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
                    vertex_buffer_create_info.size = lines.size() * sizeof(glm::vec3);
                    vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &vertex_buffer_create_info);

                    upload_buffer.upload_to_gpu(copy_pass, vertex_buffer, lines.data(),
                                                lines.size() * sizeof(glm::vec3));
                }

                /**
                 * @brief Renders the grid visualization.
                 * @param command_buffer GPU command buffer.
                 * @param render_pass GPU render pass.
                 * @param vp MVP matrix.
                 */
                void render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                 const glm::mat4 &vp)
                {
                    if (!grid_pipeline || !vertex_buffer || lines.empty())
                        return;

                    SDL_BindGPUGraphicsPipeline(render_pass, grid_pipeline);

                    SDL_GPUBufferBinding vertex_buffer_binding[] = {vertex_buffer, 0};
                    SDL_BindGPUVertexBuffers(render_pass, 0, vertex_buffer_binding, 1);

                    SDL_PushGPUVertexUniformData(command_buffer, 0, &vp[0][0], sizeof(vp));

                    SDL_DrawGPUPrimitives(render_pass, lines.size(), 1, 0, 0);
                }
        };

        /**
         * @brief Provides functions to render the event data individual points of the 3D Visualizer.
         */
        class PointsRenderer
        {
            private:
                SDL_GPUDevice *gpu_device = nullptr;
                SDL_GPUGraphicsPipeline *points_pipeline = nullptr;

            public:
                /**
                 * @brief Constructor. Initializes pipeline.
                 * @param gpu_device SDL_GPUDevice to create shader
                 */
                PointsRenderer(SDL_GPUDevice *gpu_device) : gpu_device(gpu_device)
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

                    SDL_ReleaseGPUShader(gpu_device, vs);
                    SDL_ReleaseGPUShader(gpu_device, fs);
                }

                /**
                 * @brief Destructor. Releases pipeline.
                 */
                ~PointsRenderer()
                {
                    if (points_pipeline)
                    {
                        SDL_ReleaseGPUGraphicsPipeline(gpu_device, points_pipeline);
                    }
                }

                void cpu_update(DataSource& data_source, const Parameters &params)
                {
                    // No CPU updates needed for points
                }

                void copy_pass(UploadBuffer &upload_buffer, SDL_GPUCopyPass *copy_pass, 
                              DataSource& data_source)
                {
                    // No copy operations needed - points buffer managed by scrubber
                }

                /**
                 * @brief Renders the event data points in the 3D Visualizer.
                 * @param command_buffer GPU command buffer
                 * @param render_pass GPU render pass
                 * @param vp MVP matrix
                 * @param data_source DataSource containing scrubber with points data
                 * @param params Visualizer parameters
                 */
                void render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                 const glm::mat4 &vp, DataSource& data_source,
                                 const Parameters &params);
        };

        /**
         * @brief For rendering text.
         */
        class TextRenderer
        {
            private:
                struct TextVertex
                {
                        glm::vec3 pos;
                        SDL_FColor colour;
                        glm::vec2 uv;
                };

                struct TextDrawCall
                {
                        SDL_GPUTexture *atlas_texture;
                        Uint32 index_count;
                        Uint32 index_offset;
                        Sint32 base_vertex;
                };

                SDL_GPUDevice *gpu_device = nullptr;
                SDL_GPUGraphicsPipeline *text_pipeline = nullptr;
                TTF_TextEngine *text_engine = nullptr;
                TTF_Font *font = nullptr;
                SDL_GPUSampler *sampler = nullptr;

                SDL_GPUBuffer *vertex_buffer = nullptr;
                SDL_GPUBuffer *index_buffer = nullptr;

                std::vector<TextVertex> vertices;
                std::vector<Uint32> indices;
                std::vector<TextDrawCall> draw_calls;
                std::vector<TTF_Text *> managed_text_objects;

            public:
                /**
                 * @brief Constructor. Creates pipeline and resources.
                 * @param gpu_device SDL_GPUDevice to create shader
                 */
                TextRenderer(SDL_GPUDevice *gpu_device) : gpu_device(gpu_device)
                {
                    TTF_Init();
                    text_engine = TTF_CreateGPUTextEngine(gpu_device);
                    SDL_IOStream *io = SDL_IOFromConstMem(CascadiaCode_ttf, sizeof CascadiaCode_ttf);
                    font = TTF_OpenFontIO(io, true, 24.0f);

                    SDL_GPUShaderCreateInfo vs_create_info = {0};
                    vs_create_info.code_size = sizeof text_vert;
                    vs_create_info.code = (const unsigned char *)text_vert;
                    vs_create_info.entrypoint = "main";
                    vs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
                    vs_create_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
                    vs_create_info.num_uniform_buffers = 1;
                    SDL_GPUShader *vs = SDL_CreateGPUShader(gpu_device, &vs_create_info);

                    SDL_GPUShaderCreateInfo fs_create_info = {0};
                    fs_create_info.code_size = sizeof text_frag;
                    fs_create_info.code = (const unsigned char *)text_frag;
                    fs_create_info.entrypoint = "main";
                    fs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
                    fs_create_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
                    fs_create_info.num_samplers = 1;
                    SDL_GPUShader *fs = SDL_CreateGPUShader(gpu_device, &fs_create_info);

                    SDL_GPUVertexAttribute vertex_attributes[] = {
                        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(TextVertex, pos)},
                        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(TextVertex, colour)},
                        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(TextVertex, uv)}};
                    SDL_GPUVertexBufferDescription vertex_buffer_desc = {0, sizeof(TextVertex),
                                                                         SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
                    SDL_GPUColorTargetDescription color_target_desc = {
                        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
                        .blend_state = {.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
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
                        .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                                                .enable_depth_test = true,
                                                .enable_depth_write = true},
                        .target_info = {.color_target_descriptions = &color_target_desc,
                                        .num_color_targets = 1,
                                        .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                                        .has_depth_stencil_target = true}};
                    text_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);

                    SDL_ReleaseGPUShader(gpu_device, vs);
                    SDL_ReleaseGPUShader(gpu_device, fs);

                    SDL_GPUSamplerCreateInfo sampler_info = {.min_filter = SDL_GPU_FILTER_LINEAR,
                                                             .mag_filter = SDL_GPU_FILTER_LINEAR,
                                                             .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
                                                             .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                             .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                             .address_mode_w =
                                                                 SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE};
                    sampler = SDL_CreateGPUSampler(gpu_device, &sampler_info);
                }

                /**
                 * @brief Destructor. Releases GPU resources.
                 */
                ~TextRenderer()
                {
                    vertices.clear();
                    indices.clear();
                    draw_calls.clear();

                    for (TTF_Text *text_obj : managed_text_objects)
                    {
                        TTF_DestroyText(text_obj);
                    }
                    managed_text_objects.clear();

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
                 * @brief Queues text to be rendered in 3D space.
                 */
                void add_text(const std::string &text, const glm::vec3 &position, const glm::vec3 &normal,
                              const SDL_FColor &color)
                {
                    TTF_Text *text_obj = TTF_CreateText(text_engine, font, text.c_str(), 0);
                    managed_text_objects.push_back(text_obj);

                    TTF_GPUAtlasDrawSequence *sequence = TTF_GetGPUTextDrawData(text_obj);
                    if (!sequence)
                        return;

                    const float pixel_to_world_scale = 0.0025f;

                    glm::vec3 normal_norm = glm::normalize(normal);
                    glm::vec3 forward = normal_norm;

                    glm::vec3 up_ref = glm::vec3(0.0f, 1.0f, 0.0f);
                    if (glm::abs(glm::dot(forward, up_ref)) > 0.99f)
                    {
                        up_ref = glm::vec3(0.0f, 0.0f, 1.0f);
                    }

                    glm::vec3 right = glm::normalize(glm::cross(forward, up_ref));
                    glm::vec3 up = glm::normalize(glm::cross(right, forward));

                    glm::mat4 rotation_matrix =
                        glm::mat4(right.x, up.x, -forward.x, 0.0f, right.y, up.y, -forward.y, 0.0f, right.z, up.z,
                                  -forward.z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

                    glm::mat4 model_matrix = glm::translate(glm::mat4(1.0f), position) * rotation_matrix;

                    for (TTF_GPUAtlasDrawSequence *seq = sequence; seq != NULL; seq = seq->next)
                    {
                        Uint32 current_index_offset = static_cast<Uint32>(indices.size());
                        Sint32 current_vertex_offset = static_cast<Sint32>(vertices.size());

                        for (int i = 0; i < seq->num_vertices; i++)
                        {
                            const SDL_FPoint pos2D = seq->xy[i];
                            glm::vec4 pos3D_world =
                                model_matrix *
                                glm::vec4(-pos2D.x * pixel_to_world_scale, pos2D.y * pixel_to_world_scale, 0.0f, 1.0f);

                            TextVertex vert;
                            vert.pos = glm::vec3(pos3D_world);
                            vert.colour = color;
                            vert.uv = glm::vec2(seq->uv[i].x, seq->uv[i].y);
                            vertices.push_back(vert);
                        }

                        for (int i = 0; i < seq->num_indices; i++)
                        {
                            indices.push_back(current_vertex_offset + seq->indices[i]);
                        }

                        draw_calls.push_back({.atlas_texture = seq->atlas_texture,
                                              .index_count = static_cast<Uint32>(seq->num_indices),
                                              .index_offset = current_index_offset,
                                              .base_vertex = current_vertex_offset});
                    }
                }

                /**
                 * @brief Clears text and generates labels for depth axis.
                 */
                void cpu_update(DataSource& data_source, const Parameters &params);

                /**
                 * @brief Copies text data to GPU.
                 */
                void copy_pass(UploadBuffer &upload_buffer, SDL_GPUCopyPass *copy_pass,
                              DataSource& data_source)
                {
                    if (vertices.empty() || indices.empty())
                        return;

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

                    upload_buffer.upload_to_gpu(copy_pass, vertex_buffer, vertices.data(), vertex_buffer_size);
                    upload_buffer.upload_to_gpu(copy_pass, index_buffer, indices.data(), index_buffer_size);
                }

                /**
                 * @brief Renders the text.
                 */
                void render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                 const glm::mat4 &vp, DataSource& data_source,
                                 const Parameters &params)
                {
                    if (draw_calls.empty() || !vertex_buffer || !index_buffer || !text_pipeline)
                        return;

                    SDL_BindGPUGraphicsPipeline(render_pass, text_pipeline);

                    for (const auto &call : draw_calls)
                    {
                        SDL_PushGPUVertexUniformData(command_buffer, 0, &vp[0][0], sizeof(vp));

                        SDL_GPUBufferBinding v_binding = {vertex_buffer, 0};
                        SDL_BindGPUVertexBuffers(render_pass, 0, &v_binding, 1);

                        SDL_GPUBufferBinding i_binding = {index_buffer, 0};
                        SDL_BindGPUIndexBuffer(render_pass, &i_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

                        SDL_GPUTextureSamplerBinding sampler_binding = {.texture = call.atlas_texture,
                                                                        .sampler = sampler};
                        SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);

                        SDL_DrawGPUIndexedPrimitives(render_pass, call.index_count, 1, call.index_offset,
                                                     call.base_vertex, 0);
                    }
                }
        };

        /**
         * @brief Draw frame data.
         */
        class FramesRenderer
        {
            private:
                SDL_GPUDevice *gpu_device = nullptr;
                SDL_GPUGraphicsPipeline *frames_pipeline = nullptr;
                SDL_GPUSampler *sampler = nullptr;

            public:
                /**
                 * @brief Constructor. Initializes pipeline.
                 * @param gpu_device GPU device
                 */
                FramesRenderer(SDL_GPUDevice *gpu_device) : gpu_device(gpu_device)
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

                    SDL_ReleaseGPUShader(gpu_device, vs);
                    SDL_ReleaseGPUShader(gpu_device, fs);

                    SDL_GPUSamplerCreateInfo sampler_info = {};
                    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
                    sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
                    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
                    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
                    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
                    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                    sampler_info.mip_lod_bias = 0.0f;
                    sampler_info.min_lod = -1000.0f;
                    sampler_info.max_lod = 1000.0f;
                    sampler_info.enable_anisotropy = false;
                    sampler_info.max_anisotropy = 1.0f;
                    sampler_info.enable_compare = false;
                    sampler = SDL_CreateGPUSampler(gpu_device, &sampler_info);
                }

                /**
                 * @brief Destructor. Releases necessary GPU resources.
                 */
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

                void cpu_update(DataSource& data_source, const Parameters &params)
                {
                }

                void copy_pass(UploadBuffer &upload_buffer, SDL_GPUCopyPass *copy_pass,
                              DataSource& data_source)
                {
                }

                /**
                 * @brief Renders the frames.
                 */
                void render_pass(SDL_GPUCommandBuffer *command_buffer, SDL_GPURenderPass *render_pass,
                                 const glm::mat4 &vp, DataSource& data_source,
                                 const Parameters &params);
        };

        // Camera
        Camera camera;
        glm::vec3 box_min;
        glm::vec3 box_max;

        // GPU
        SDL_GPUDevice *gpu_device = nullptr;
        UploadBuffer upload_buffer;

        GridRenderer *grid_renderer = nullptr;
        PointsRenderer *points_renderer = nullptr;
        TextRenderer *text_renderer = nullptr;
        FramesRenderer *frames_renderer = nullptr;

    public:
        /**
         * @brief Constructor. Initializes pipelines only.
         * @param gpu_device SDL_GPUDevice to create texture on
         */
        Visualizer(GPUDevice& gpu_device)
            : gpu_device(gpu_device.get_SDL_device()),
              upload_buffer(gpu_device.get_SDL_device())
        {
            camera = Camera(glm::vec3(0.0f, 0.0f, 0.0f), 4.0f, 45.0f, 1920.0f / 1200.0f, 0.1f, 1000.0f);

            grid_renderer = new GridRenderer(this->gpu_device);
            points_renderer = new PointsRenderer(this->gpu_device);
            text_renderer = new TextRenderer(this->gpu_device);
            frames_renderer = new FramesRenderer(this->gpu_device);
        }

        /**
         * @brief Destructor, release necessary resources.
         */
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
        }

        // Camera control methods for GUI
        void rotate_camera(float x_offset, float y_offset)
        {
            camera.processMouseMovement(x_offset, y_offset);
        }

        void zoom_camera(float scroll_delta)
        {
            camera.processMouseScroll(scroll_delta);
        }

        /**
         * @brief All-encompassing render method that handles CPU updates, copy pass, and rendering.
         * @param data_source Shared pointer to DataSource containing event data and scrubber
         */
        void render(std::shared_ptr<DataSource> data_source);
        void render(DataSource& data_source);
    };

#endif