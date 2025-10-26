#pragma once
#ifndef DIGITAL_CODED_EXPOSURE_HH
#define DIGITAL_CODED_EXPOSURE_HH

#include "pch.hh"

#include "Camera.hh"
#include "EventData.hh"
#include "ParameterStore.hh"
#include "RenderTarget.hh"
#include "UploadBuffer.hh"

class DigitalCodedExposure
{
    public:
        DigitalCodedExposure(ParameterStore &parameter_store,
                             std::unordered_map<std::string, RenderTarget> &render_targets, EventData &event_data,
                             SDL_Window *window, SDL_GPUDevice *gpu_device, UploadBuffer *upload_buffer,
                             SDL_GPUCopyPass *copy_pass)
            : parameter_store(parameter_store), render_targets(render_targets), event_data(event_data), window(window),
              gpu_device(gpu_device) ~DigitalCodedExposure()
        {
        }

        bool init(int w, int h)
        {
            width = w;
            height = h;
        }

        bool event_handler(SDL_Event *event)
        {
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
};

#endif