#pragma once

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <print>
#ifndef SPINNING_CUBE_HH
#define SPINNING_CUBE_HH

#include "pch.hh"

#include "RenderTarget.hh"
#include "UploadBuffer.hh"

#include "shaders/spinning_cube_frag.h"
#include "shaders/spinning_cube_vert.h"

class SpinningCube
{
    private:
        struct Vertex
        {
                float x, y, z;
                float r, g, b, a;
        };

        static constexpr const Vertex cubeVertices[] = {
            // Front face (red)
            {-0.5f, -0.5f, 0.5f, 1.f, 0.f, 0.f, 1.f},
            {0.5f, -0.5f, 0.5f, 1.f, 0.f, 0.f, 1.f},
            {0.5f, 0.5f, 0.5f, 1.f, 0.f, 0.f, 1.f},
            {-0.5f, -0.5f, 0.5f, 1.f, 0.f, 0.f, 1.f},
            {0.5f, 0.5f, 0.5f, 1.f, 0.f, 0.f, 1.f},
            {-0.5f, 0.5f, 0.5f, 1.f, 0.f, 0.f, 1.f},
            // Back face (green)
            {-0.5f, -0.5f, -0.5f, 0.f, 1.f, 0.f, 1.f},
            {-0.5f, 0.5f, -0.5f, 0.f, 1.f, 0.f, 1.f},
            {0.5f, 0.5f, -0.5f, 0.f, 1.f, 0.f, 1.f},
            {-0.5f, -0.5f, -0.5f, 0.f, 1.f, 0.f, 1.f},
            {0.5f, 0.5f, -0.5f, 0.f, 1.f, 0.f, 1.f},
            {0.5f, -0.5f, -0.5f, 0.f, 1.f, 0.f, 1.f},
            // Left face (blue)
            {-0.5f, -0.5f, -0.5f, 0.f, 0.f, 1.f, 1.f},
            {-0.5f, -0.5f, 0.5f, 0.f, 0.f, 1.f, 1.f},
            {-0.5f, 0.5f, 0.5f, 0.f, 0.f, 1.f, 1.f},
            {-0.5f, -0.5f, -0.5f, 0.f, 0.f, 1.f, 1.f},
            {-0.5f, 0.5f, 0.5f, 0.f, 0.f, 1.f, 1.f},
            {-0.5f, 0.5f, -0.5f, 0.f, 0.f, 1.f, 1.f},
            // Right face (yellow)
            {0.5f, -0.5f, -0.5f, 1.f, 1.f, 0.f, 1.f},
            {0.5f, 0.5f, 0.5f, 1.f, 1.f, 0.f, 1.f},
            {0.5f, -0.5f, 0.5f, 1.f, 1.f, 0.f, 1.f},
            {0.5f, -0.5f, -0.5f, 1.f, 1.f, 0.f, 1.f},
            {0.5f, 0.5f, -0.5f, 1.f, 1.f, 0.f, 1.f},
            {0.5f, 0.5f, 0.5f, 1.f, 1.f, 0.f, 1.f},
            // Top face (cyan)
            {-0.5f, 0.5f, 0.5f, 0.f, 1.f, 1.f, 1.f},
            {0.5f, 0.5f, 0.5f, 0.f, 1.f, 1.f, 1.f},
            {0.5f, 0.5f, -0.5f, 0.f, 1.f, 1.f, 1.f},
            {-0.5f, 0.5f, 0.5f, 0.f, 1.f, 1.f, 1.f},
            {0.5f, 0.5f, -0.5f, 0.f, 1.f, 1.f, 1.f},
            {-0.5f, 0.5f, -0.5f, 0.f, 1.f, 1.f, 1.f},
            // Bottom face (magenta)
            {-0.5f, -0.5f, 0.5f, 1.f, 0.f, 1.f, 1.f},
            {-0.5f, -0.5f, -0.5f, 1.f, 0.f, 1.f, 1.f},
            {0.5f, -0.5f, -0.5f, 1.f, 0.f, 1.f, 1.f},
            {-0.5f, -0.5f, 0.5f, 1.f, 0.f, 1.f, 1.f},
            {0.5f, -0.5f, -0.5f, 1.f, 0.f, 1.f, 1.f},
            {0.5f, -0.5f, 0.5f, 1.f, 0.f, 1.f, 1.f}};

        glm::mat4 mvp;
        float rotation_time = 0.0f;

        SDL_GPUDevice *gpu_device = nullptr;
        SDL_GPUBuffer *vertex_buffer = nullptr;
        SDL_GPUGraphicsPipeline *pipeline = nullptr;

        std::unordered_map<std::string, RenderTarget> &render_targets;

    public:
        SpinningCube(SDL_GPUDevice *gpu_device, UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass,
                     std::unordered_map<std::string, RenderTarget> &render_targets)
            : gpu_device(gpu_device), render_targets(render_targets)
        {
            SDL_GPUShaderCreateInfo vs_create_info = {.code_size = sizeof spinning_cube_vert,
                                                      .code = (const unsigned char *)spinning_cube_vert,
                                                      .entrypoint = "main",
                                                      .format = SDL_GPU_SHADERFORMAT_SPIRV,
                                                      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                                                      .num_samplers = 0,
                                                      .num_storage_textures = 0,
                                                      .num_storage_buffers = 0,
                                                      .num_uniform_buffers = 1};
            SDL_GPUShader *vs = SDL_CreateGPUShader(gpu_device, &vs_create_info);

            SDL_GPUShaderCreateInfo fs_create_info = {.code_size = sizeof spinning_cube_frag,
                                                      .code = (const unsigned char *)spinning_cube_frag,
                                                      .entrypoint = "main",
                                                      .format = SDL_GPU_SHADERFORMAT_SPIRV,
                                                      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                                                      .num_samplers = 0,
                                                      .num_storage_textures = 0,
                                                      .num_storage_buffers = 0,
                                                      .num_uniform_buffers = 0};
            SDL_GPUShader *fs = SDL_CreateGPUShader(gpu_device, &fs_create_info);

            SDL_GPUBufferCreateInfo vertex_buffer_create_info = {.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                                                                 .size = sizeof(cubeVertices)};
            vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &vertex_buffer_create_info);
            upload_buffer->upload_to_gpu(copy_pass, vertex_buffer, cubeVertices, sizeof(cubeVertices));

            SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {
                .vertex_shader = vs,
                .fragment_shader = fs,
                .vertex_input_state =
                    (SDL_GPUVertexInputState){
                        .vertex_buffer_descriptions =
                            (SDL_GPUVertexBufferDescription[]){0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0},
                        .num_vertex_buffers = 1,
                        .vertex_attributes =
                            (SDL_GPUVertexAttribute[]){0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0, 1, 0,
                                                       SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, sizeof(float) * 3},
                        .num_vertex_attributes = 2},
                .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
                .depth_stencil_state = {
                    .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                    .enable_depth_test = true,
                    .enable_depth_write = true
                },
                .target_info = {.color_target_descriptions =
                                    (SDL_GPUColorTargetDescription[]){SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM},
                                .num_color_targets = 1,
                                .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                                .has_depth_stencil_target = true}};

            pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipelineInfo);
            SDL_ReleaseGPUShader(gpu_device, vs);
            SDL_ReleaseGPUShader(gpu_device, fs);

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
            render_targets["SpinningCubeColor"] = {SDL_CreateGPUTexture(gpu_device, &color_create_info), 1920, 1080};

            SDL_GPUTextureCreateInfo depth_create_info = {
                .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
                .width = 1920,
                .height = 1080,
                .layer_count_or_depth = 1,
                .num_levels = 1,
                .sample_count = SDL_GPU_SAMPLECOUNT_1,
            };
            render_targets["SpinningCubeDepth"] = {SDL_CreateGPUTexture(gpu_device, &depth_create_info), 1920, 1080};
        }

        ~SpinningCube()
        {
            SDL_ReleaseGPUTexture(gpu_device, render_targets["SpinningCubeDepth"].texture);
            SDL_ReleaseGPUTexture(gpu_device, render_targets["SpinningCubeColor"].texture);
            SDL_ReleaseGPUGraphicsPipeline(gpu_device, pipeline);

            SDL_ReleaseGPUBuffer(gpu_device, vertex_buffer);
        }

        void event_handler(SDL_Event *event)
        {
            // from the ImGui io, we can use the window name to figure out
            // if this "window" is being selected
            ImGuiIO &io = ImGui::GetIO();
        }

        void update()
        {
            // Update rotation time (assuming 60 FPS, adjust as needed)
            rotation_time += 0.0016f; // ~60 FPS
            
            // Create model matrix with rotation
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::rotate(model, rotation_time, glm::vec3(1.0f, 1.0f, 0.0f)); // Rotate around X+Y axis
            
            // Create view matrix (camera positioned at (0, 0, 3) looking at origin)
            glm::mat4 view = glm::lookAt(
                glm::vec3(0.0f, 0.0f, 3.0f), // Camera position
                glm::vec3(0.0f, 0.0f, 0.0f), // Look at origin
                glm::vec3(0.0f, 1.0f, 0.0f)  // Up vector
            );
            
            // Create perspective projection matrix
            glm::mat4 projection = glm::perspective(
                glm::radians(45.0f), // Field of view
                1920.0f / 1080.0f,   // Aspect ratio
                0.1f,                // Near plane
                100.0f               // Far plane
            );
            
            // Combine matrices: MVP = Projection * View * Model
            mvp = projection * view * model;
        }

        void sync_to_gpu(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        {

        }

        void render(SDL_GPUCommandBuffer *command_buffer)
        {
            SDL_GPUColorTargetInfo color_target_info = {0};
            color_target_info.texture = render_targets["SpinningCubeColor"].texture;
            color_target_info.clear_color = (SDL_FColor){1.0f, 1.0f, 1.0f, 1.0f};
            color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
            color_target_info.store_op = SDL_GPU_STOREOP_STORE;
            color_target_info.mip_level = 0;
            color_target_info.layer_or_depth_plane = 0;
            color_target_info.cycle = false;

            SDL_GPUDepthStencilTargetInfo depth_target_info = {.texture = render_targets["SpinningCubeDepth"].texture,
                                                               .clear_depth = 1.0f,
                                                               .load_op = SDL_GPU_LOADOP_CLEAR,
                                                               .store_op = SDL_GPU_STOREOP_DONT_CARE,
                                                               .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
                                                               .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
                                                               .cycle = false,
                                                               .clear_stencil = 0};
        

            SDL_GPURenderPass *render_pass =
                SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, &depth_target_info);
             SDL_BindGPUGraphicsPipeline(render_pass, pipeline);
             SDL_BindGPUVertexBuffers(render_pass, 0, (SDL_GPUBufferBinding[]){vertex_buffer, 0}, 1);
             SDL_PushGPUVertexUniformData(command_buffer, 0, &mvp[0][0], sizeof(mvp));
             SDL_DrawGPUPrimitives(render_pass, 36, 1, 0, 0);
             SDL_EndGPURenderPass(render_pass);
        }
};

#endif