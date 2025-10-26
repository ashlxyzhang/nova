#pragma once

#include <SDL3/SDL_gpu.h>
#ifndef SCRUBBER_HH
#define SCRUBBER_HH

#include "pch.hh"

#include "EventData.hh"
#include "ParameterStore.hh"
#include "UploadBuffer.hh"

class Scrubber
{
    public:
        enum class ScrubberType : uint8_t
        {
            EVENT,
            TIME,
        };

        enum class ScrubberMode : uint8_t
        {
            PAUSED,
            PLAYING,
            LATEST,
        };

    private:
        // so the idea is that we give the user two options for scrubbing through the data,
        // an event and a time based system. however at the end of the day
        // everything is an index into an array, so we need to keep all these values in
        // sync in this class

        // since we are looking for streaming data support, lets make the window always face behind the
        // current stamp

        std::size_t lower_index = 0;
        std::size_t current_index = 0;
        std::size_t index_step = 0;
        std::size_t index_window = 0;

        float lower_time = 0.0f;
        float current_time = 0.0f;
        float time_step = 0.0f;
        float time_window = 0.0f;

        ParameterStore &parameter_store;
        EventData *event_data = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;

        SDL_GPUBuffer *points_buffer = nullptr;
        std::size_t points_buffer_size = 0;

        SDL_GPUTexture *textures = nullptr;
        SDL_GPUBuffer *textures_timestamps = nullptr;

    public:
        Scrubber(ParameterStore &parameter_store, EventData *event_data, SDL_GPUDevice *gpu_device)
            : parameter_store(parameter_store), event_data(event_data), gpu_device(gpu_device)
        {
            parameter_store.add("scrubber.mode", ScrubberMode::PAUSED);
            parameter_store.add("scrubber.type", ScrubberType::TIME);

            parameter_store.add("scrubber.current_index", current_index);
            parameter_store.add("scrubber.index_window", index_window);
            parameter_store.add("scrubber.index_step", index_step);
            parameter_store.add("scrubber.min_index", 0ULL);
            parameter_store.add("scrubber.max_index", 0ULL);

            parameter_store.add("scrubber.current_time", current_time);
            parameter_store.add("scrubber.time_window", time_window);
            parameter_store.add("scrubber.time_step", time_step);
            parameter_store.add("scrubber.min_time", 0.0f);
            parameter_store.add("scrubber.max_time", 0.0f);
        }

        ~Scrubber()
        {
            if (points_buffer)
            {
                SDL_ReleaseGPUBuffer(gpu_device, points_buffer);
            }
            if (textures)
            {
                SDL_ReleaseGPUTexture(gpu_device, textures);
            }
            if (textures_timestamps)
            {
                SDL_ReleaseGPUBuffer(gpu_device, textures_timestamps);
            }
        }

        void cpu_update()
        {
            current_index = parameter_store.get<std::size_t>("scrubber.current_index");
            index_window = parameter_store.get<std::size_t>("scrubber.index_window");
            index_step = parameter_store.get<std::size_t>("scrubber.index_step");

            event_data->lock_data_vectors();

            parameter_store.add("scrubber.min_index", 0ULL);
            parameter_store.add("scrubber.max_index", event_data->get_evt_vector_ref(true).size() - 1);

            if (parameter_store.get<ScrubberType>("scrubber.type") == ScrubberType::TIME)
            {
                if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::PAUSED)
                {
                }
                else if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::PLAYING)
                {
                }
                else if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::LATEST)
                {
                }
            }

            if (parameter_store.get<ScrubberType>("scrubber.type") == ScrubberType::EVENT)
            {
                if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::PAUSED)
                {
                    current_index = std::clamp(current_index, 0ULL, event_data->get_evt_vector_ref(true).size() - 1);
                    index_window = std::clamp(index_window, 0ULL, event_data->get_evt_vector_ref(true).size() - 1);
                    index_step = std::clamp(index_step, 0ULL, event_data->get_evt_vector_ref(true).size() - 1);
                    lower_index = std::max(0ULL, current_index - index_window);
                }
                else if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::PLAYING)
                {
                    index_step = std::clamp(index_step, 0ULL, event_data->get_evt_vector_ref(true).size() - 1);
                    index_window = std::clamp(index_window, 0ULL, event_data->get_evt_vector_ref(true).size() - 1);
                    current_index = std::clamp(current_index + index_step, 0ULL, event_data->get_evt_vector_ref(true).size() - 1);
                    lower_index = std::max(0ULL, current_index - index_window);
                }
                else if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::LATEST)
                {
                    current_index = event_data->get_evt_vector_ref(true).size() - 1;
                    index_window = std::clamp(index_window, 0ULL, event_data->get_evt_vector_ref(true).size() - 1);
                    index_step = std::clamp(index_step, 0ULL, event_data->get_evt_vector_ref(true).size() - 1);
                    lower_index = std::max(0ULL, current_index - index_window);
                }
            }

            event_data->unlock_data_vectors();
        }

        void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        {
            if (!event_data || !upload_buffer || !copy_pass)
            {
                return;
            }

            event_data->lock_data_vectors();

            // Get the data we need to copy
            const std::vector<glm::vec4> &evt_vector = event_data->get_evt_vector_ref(true);
            
            // Calculate how many points we need to upload (from lower_index to current_index)
            std::size_t num_points = 0;
            if (current_index >= lower_index)
            {
                num_points = current_index - lower_index + 1;
            }
            
            // Clamp to the actual size of the vector
            num_points = std::min(num_points, evt_vector.size() - lower_index);

            event_data->unlock_data_vectors();

            // If we have no points to upload, skip
            if (num_points == 0)
            {
                return;
            }

            // Delete old buffer if it exists
            if (points_buffer)
            {
                SDL_ReleaseGPUBuffer(gpu_device, points_buffer);
                points_buffer = nullptr;
            }

            // Calculate the size needed for the buffer
            points_buffer_size = num_points * sizeof(glm::vec4);

            // Create new buffer
            SDL_GPUBufferCreateInfo buffer_create_info = {0};
            buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
            buffer_create_info.size = points_buffer_size;
            points_buffer = SDL_CreateGPUBuffer(gpu_device, &buffer_create_info);

            // Upload data to the new buffer
            event_data->lock_data_vectors();
            const glm::vec4 *data_ptr = evt_vector.data() + lower_index;
            upload_buffer->upload_to_gpu(copy_pass, points_buffer, data_ptr, points_buffer_size);
            event_data->unlock_data_vectors();
        }

        // return the points buffer
        SDL_GPUBuffer *get_points_buffer()
        {
            return points_buffer;
        }

        // return the size of the points buffer
        std::size_t get_points_buffer_size()
        {
            return points_buffer_size / sizeof(glm::vec4);
        }
};

#endif
