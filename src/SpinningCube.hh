#pragma once

#ifndef SPINNING_CUBE_HH
#define SPINNING_CUBE_HH

#include "pch.hh"

#include "Camera.hh"
#include "RenderTarget.hh"
#include "UploadBuffer.hh"

#include "shaders/spinning_cube/spinning_cube_frag.h"
#include "shaders/spinning_cube/spinning_cube_vert.h"

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
        Camera camera;

        SDL_GPUDevice *gpu_device = nullptr;
        SDL_GPUBuffer *vertex_buffer = nullptr;
        SDL_GPUGraphicsPipeline *pipeline = nullptr;
        SDL_Window *window = nullptr;

        std::unordered_map<std::string, RenderTarget> &render_targets;

        // Mouse state for camera orbiting
        bool is_mouse_dragging = false;
        float last_mouse_x = 0.0f;
        float last_mouse_y = 0.0f;
        bool cursor_captured = false;

    public:
        SpinningCube(SDL_GPUDevice *gpu_device, UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass,
                     std::unordered_map<std::string, RenderTarget> &render_targets, SDL_Window *window)
            : gpu_device(gpu_device), render_targets(render_targets), window(window),
              camera(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f, 45.0f, 1920.0f / 1080.0f,
                     0.1f, 100.0f)
        {
            // create the vertex shader, this can in the future be replaced by sdl_shadercross
            SDL_GPUShaderCreateInfo vs_create_info = {0};
            vs_create_info.code_size = sizeof spinning_cube_vert;
            vs_create_info.code = (const unsigned char *)spinning_cube_vert;
            vs_create_info.entrypoint = "main";
            vs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            vs_create_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
            vs_create_info.num_samplers = 0;
            vs_create_info.num_storage_textures = 0;
            vs_create_info.num_storage_buffers = 0;
            vs_create_info.num_uniform_buffers = 1;
            SDL_GPUShader *vs = SDL_CreateGPUShader(gpu_device, &vs_create_info);

            // create the fragment shader
            SDL_GPUShaderCreateInfo fs_create_info = {0};
            fs_create_info.code_size = sizeof spinning_cube_frag;
            fs_create_info.code = (const unsigned char *)spinning_cube_frag;
            fs_create_info.entrypoint = "main";
            fs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            fs_create_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
            fs_create_info.num_samplers = 0;
            fs_create_info.num_storage_textures = 0;
            fs_create_info.num_storage_buffers = 0;
            fs_create_info.num_uniform_buffers = 0;
            SDL_GPUShader *fs = SDL_CreateGPUShader(gpu_device, &fs_create_info);

            // create the vertex buffer, this is the buffer that will store the vertex data
            SDL_GPUBufferCreateInfo vertex_buffer_create_info = {0};
            vertex_buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            vertex_buffer_create_info.size = sizeof(cubeVertices);
            vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &vertex_buffer_create_info);
            upload_buffer->upload_to_gpu(copy_pass, vertex_buffer, cubeVertices, sizeof(cubeVertices));

            // create the graphics pipeline, this is the pipeline that will be used to render the vertex data
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
                .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                                        .enable_depth_test = true,
                                        .enable_depth_write = true},
                .target_info = {.color_target_descriptions =
                                    (SDL_GPUColorTargetDescription[]){SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM},
                                .num_color_targets = 1,
                                .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
                                .has_depth_stencil_target = true}};

            pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipelineInfo);
            SDL_ReleaseGPUShader(gpu_device, vs);
            SDL_ReleaseGPUShader(gpu_device, fs);

            // create the color texture, this is the texture that will store the color data
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

            // create the depth texture, this is the texture that will store the depth data
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

        // if an event was handled, return true, otherwise return false
        bool event_handler(SDL_Event *event)
        {
            if (render_targets["SpinningCubeColor"].is_focused == true)
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
                        float y_offset = -event->motion.yrel; // Reversed since y-coordinates go from bottom to top

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
                render_targets["SpinningCubeColor"].is_focused = false;
                return true;
            }
            return false;
        }

        void cpu_update()
        {
            // Update rotation time (assuming 60 FPS, adjust as needed)
            rotation_time += 0.016f; // ~60 FPS

            if (render_targets["SpinningCubeColor"].is_focused == true)
            {
                // Process keyboard input for camera movement
                float delta_time = 0.016f; // ~60 FPS
                const bool *key_states = SDL_GetKeyboardState(NULL);

                if (key_states[SDL_SCANCODE_W])
                {
                    camera.processKeyboard(Camera::MovementType::FORWARD, delta_time);
                }
                if (key_states[SDL_SCANCODE_S])
                {
                    camera.processKeyboard(Camera::MovementType::BACKWARD, delta_time);
                }
                if (key_states[SDL_SCANCODE_A])
                {
                    camera.processKeyboard(Camera::MovementType::LEFT, delta_time);
                }
                if (key_states[SDL_SCANCODE_D])
                {
                    camera.processKeyboard(Camera::MovementType::RIGHT, delta_time);
                }
            }

            // Create model matrix with rotation
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::rotate(model, rotation_time, glm::vec3(1.0f, 1.0f, 0.0f)); // Rotate around X+Y axis

            // Get view and projection matrices from camera
            glm::mat4 view = camera.getViewMatrix();
            glm::mat4 projection = camera.getProjectionMatrix();

            // Combine matrices: MVP = Projection * View * Model
            mvp = projection * view * model;
        }

        // copy the data from cpu to gpu, we do this one first so data gets ready before the compute pass,
        // copy pass is special as we are not requried to pass in dependencies when we create the pass.
        void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        {
        }

        // do compute stuff here, for the architecture of this app, we can assume
        // that compute always goes before the render pass
        void compute_pass(SDL_GPUCommandBuffer *command_buffer)
        {
        }

        // do the actual rendering
        void render_pass(SDL_GPUCommandBuffer *command_buffer)
        {
            // create the color target info, this is the texture that will store the color data
            SDL_GPUColorTargetInfo color_target_info = {0};
            color_target_info.texture = render_targets["SpinningCubeColor"].texture;
            color_target_info.clear_color = (SDL_FColor){1.0f, 1.0f, 1.0f, 1.0f};
            color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
            color_target_info.store_op = SDL_GPU_STOREOP_STORE;
            color_target_info.mip_level = 0;
            color_target_info.layer_or_depth_plane = 0;
            color_target_info.cycle = false;

            // create the depth target info, this is the texture that will store the depth data
            SDL_GPUDepthStencilTargetInfo depth_target_info = {0};
            depth_target_info.texture = render_targets["SpinningCubeDepth"].texture;
            depth_target_info.clear_depth = 1.0f;
            depth_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
            depth_target_info.store_op = SDL_GPU_STOREOP_DONT_CARE;
            depth_target_info.cycle = false;

            // create the render pass, this is the pass that will be used to render the vertex data
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