#pragma once

#ifndef VISUALIZER_HH
#define VISUALIZER_HH

#include "pch.hh"

#include "Camera.hh"
#include "EventData.hh"
#include "ParameterStore.hh"
#include "RenderTarget.hh"
#include "UploadBuffer.hh"

#include "shaders/visualizer/grid/grid_vert.h"
#include "shaders/visualizer/grid/grid_frag.h"
#include "shaders/visualizer/points/points_vert.h"
#include "shaders/visualizer/points/points_frag.h"

class Visualizer
{
    private:
        Camera camera;
        glm::vec3 box_min;
        glm::vec3 box_max;

        ParameterStore &parameter_store;
        std::unordered_map<std::string, RenderTarget> &render_targets;
        EventData &event_data;
        
        SDL_Window *window = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;

        SDL_GPUGraphicsPipeline *grid_pipeline = nullptr;
        SDL_GPUGraphicsPipeline *points_pipeline = nullptr;
        SDL_GPUGraphicsPipeline *frame_pipeline = nullptr;
        SDL_GPUGraphicsPipeline *text_pipeline = nullptr;

        void create_grid_pipeline()
        {
            // Create vertex shader
            SDL_GPUShaderCreateInfo vs_create_info = {0};
            vs_create_info.code = (const Uint8*)grid_vert;
            vs_create_info.code_size = sizeof(grid_vert);
            vs_create_info.entrypoint = "main";
            vs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            vs_create_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
            vs_create_info.num_samplers = 0;
            vs_create_info.num_storage_textures = 0;
            vs_create_info.num_storage_buffers = 0;
            vs_create_info.num_uniform_buffers = 1;
            SDL_GPUShader *vs = SDL_CreateGPUShader(gpu_device, &vs_create_info);

            // Create fragment shader
            SDL_GPUShaderCreateInfo fs_create_info = {0};
            fs_create_info.code = (const Uint8*)grid_frag;
            fs_create_info.code_size = sizeof(grid_frag);
            fs_create_info.entrypoint = "main";
            fs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            fs_create_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
            fs_create_info.num_samplers = 0;
            fs_create_info.num_storage_textures = 0;
            fs_create_info.num_storage_buffers = 0;
            fs_create_info.num_uniform_buffers = 0;
            SDL_GPUShader *fs = SDL_CreateGPUShader(gpu_device, &fs_create_info);

            // Create graphics pipeline
            SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {0};
            pipeline_info.vertex_shader = vs;
            pipeline_info.fragment_shader = fs;
            pipeline_info.vertex_input_state = {
                .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]){
                    {0, sizeof(glm::vec4) * 2, SDL_GPU_VERTEXINPUTRATE_VERTEX, 0}
                },
                .num_vertex_buffers = 1,
                .vertex_attributes = (SDL_GPUVertexAttribute[]){
                    {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
                    {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, sizeof(glm::vec4)}
                },
                .num_vertex_attributes = 2
            };
            pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
            SDL_GPURasterizerState rasterizer_state = {};
            rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
            rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
            rasterizer_state.enable_depth_bias = false;
            rasterizer_state.enable_depth_clip = true;
            pipeline_info.rasterizer_state = rasterizer_state;
            SDL_GPUMultisampleState multisample_state = {};
            multisample_state.enable_mask = false;
            pipeline_info.multisample_state = multisample_state;
            SDL_GPUDepthStencilState depth_stencil_state = {};
            depth_stencil_state.enable_depth_test = true;
            depth_stencil_state.enable_depth_write = true;
            depth_stencil_state.enable_stencil_test = false;
            depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
            pipeline_info.depth_stencil_state = depth_stencil_state;
            SDL_GPUColorTargetDescription color_target_desc = {
                .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                .blend_state = {
                    .enable_blend = false
                }
            };
            
            SDL_GPUGraphicsPipelineTargetInfo target_info = {};
            target_info.num_color_targets = 1;
            target_info.color_target_descriptions = &color_target_desc;
            target_info.has_depth_stencil_target = true;
            target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            pipeline_info.target_info = target_info;

            grid_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);
        }

        void create_points_pipeline()
        {
            // Create vertex shader
            SDL_GPUShaderCreateInfo vs_create_info = {0};
            vs_create_info.code = (const Uint8*)points_vert;
            vs_create_info.code_size = sizeof(points_vert);
            vs_create_info.entrypoint = "main";
            vs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            vs_create_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
            vs_create_info.num_samplers = 0;
            vs_create_info.num_storage_textures = 0;
            vs_create_info.num_storage_buffers = 0;
            vs_create_info.num_uniform_buffers = 1;
            SDL_GPUShader *vs = SDL_CreateGPUShader(gpu_device, &vs_create_info);

            // Create fragment shader
            SDL_GPUShaderCreateInfo fs_create_info = {0};
            fs_create_info.code = (const Uint8*)points_frag;
            fs_create_info.code_size = sizeof(points_frag);
            fs_create_info.entrypoint = "main";
            fs_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            fs_create_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
            fs_create_info.num_samplers = 0;
            fs_create_info.num_storage_textures = 0;
            fs_create_info.num_storage_buffers = 0;
            fs_create_info.num_uniform_buffers = 0;
            SDL_GPUShader *fs = SDL_CreateGPUShader(gpu_device, &fs_create_info);

            // Create graphics pipeline
            SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {0};
            pipeline_info.vertex_shader = vs;
            pipeline_info.fragment_shader = fs;
            pipeline_info.vertex_input_state = {
                .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]){
                    {0, sizeof(glm::vec4), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0}
                },
                .num_vertex_buffers = 1,
                .vertex_attributes = (SDL_GPUVertexAttribute[]){
                    {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0}
                },
                .num_vertex_attributes = 1
            };
            pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_POINTLIST;
            SDL_GPURasterizerState rasterizer_state = {};
            rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
            rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
            rasterizer_state.enable_depth_bias = false;
            rasterizer_state.enable_depth_clip = true;
            pipeline_info.rasterizer_state = rasterizer_state;
            SDL_GPUMultisampleState multisample_state = {};
            multisample_state.enable_mask = false;
            pipeline_info.multisample_state = multisample_state;
            SDL_GPUDepthStencilState depth_stencil_state = {};
            depth_stencil_state.enable_depth_test = true;
            depth_stencil_state.enable_depth_write = true;
            depth_stencil_state.enable_stencil_test = false;
            depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
            pipeline_info.depth_stencil_state = depth_stencil_state;
            SDL_GPUColorTargetDescription color_target_desc = {
                .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                .blend_state = {
                    .enable_blend = false
                }
            };
            
            SDL_GPUGraphicsPipelineTargetInfo target_info = {};
            target_info.num_color_targets = 1;
            target_info.color_target_descriptions = &color_target_desc;
            target_info.has_depth_stencil_target = true;
            target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            pipeline_info.target_info = target_info;

            points_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);
        }

    public:
        Visualizer(ParameterStore &parameter_store, std::unordered_map<std::string, RenderTarget> &render_targets,
                   EventData &event_data, SDL_Window *window, SDL_GPUDevice *gpu_device, UploadBuffer *upload_buffer,
                   SDL_GPUCopyPass *copy_pass)
            : parameter_store(parameter_store), render_targets(render_targets), event_data(event_data), window(window),
              gpu_device(gpu_device)
        {
            create_grid_pipeline();
            create_points_pipeline();
        }

        bool event_handler(SDL_Event *event)
        {
            return false;
        }

        void cpu_update()
        {
        }

        void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        {
        }

        void compute_pass(SDL_GPUCommandBuffer *command_buffer)
        {
        }

        void render_pass(SDL_GPUCommandBuffer *command_buffer)
        {
        }
};

#endif
