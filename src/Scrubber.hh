#pragma once

#ifndef SCRUBBER_HH
#define SCRUBBER_HH

#include "pch.hh"

#include "EventData.hh"
#include "ParameterStore.hh"
#include "UploadBuffer.hh"

// this class will handle all things to do with scrubbing and window size of particles.
// since at the end of the day, everything is related to values in an vector,
// we need a way to easily switch between time and event domains
class Scrubber 
{
    public:
        enum class ScrubberType
        {
            TIME,
            EVENT,
        };

        enum class ScrubberMode 
        {
            PAUSED,
            PLAYING,
            LATEST,
        };

    private:
        ParameterStore &parameter_store;
        EventData *event_data = nullptr;
        SDL_GPUDevice *gpu_device = nullptr;

        // not sure how to deal with time yet, leave it for later
        float lower_time = 0.0f;
        float current_time = 0.0f;
        float upper_time = 0.0f;
        float time_window = 0.0f;

        std::size_t lower_index = 0;
        std::size_t current_index = 0;
        std::size_t upper_index = 0;
        std::size_t index_window = 50;

        SDL_GPUBuffer *points_buffer = nullptr;
        SDL_GPUTexture *textures = nullptr;
        SDL_GPUBuffer *textures_timestamps = nullptr;
        
        // Track what scrubber type we're using
        ScrubberType scrubber_type = ScrubberType::EVENT;
        
        // Track current data size for change detection
        std::size_t last_data_size = 0;

    public:

    
        Scrubber(ParameterStore &parameter_store, EventData *event_data, SDL_GPUDevice *gpu_device) : parameter_store(parameter_store), event_data(event_data), gpu_device(gpu_device)
        {
            parameter_store.add("scrubber.mode", ScrubberMode::PAUSED);
            parameter_store.add("scrubber.type", scrubber_type);

            parameter_store.add("scrubber.current_index", current_index);
            parameter_store.add("scrubber.index_window", index_window);

            parameter_store.add("scrubber.current_time", current_time);
            parameter_store.add("scrubber.time_window", time_window);
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
            if (!event_data)
                return;

            // Get current parameters
            ScrubberMode mode = parameter_store.get<ScrubberMode>("scrubber.mode");
            scrubber_type = parameter_store.get<ScrubberType>("scrubber.type");
            
            // Update current based on mode
            if (mode == ScrubberMode::PLAYING)
            {
                if (scrubber_type == ScrubberType::EVENT)
                {
                    current_index++;
                    parameter_store.add("scrubber.current_index", current_index);
                }
                else // TIME
                {
                    // For time-based, we'd increment time_window worth of time
                    // For now, just increment a small amount
                    current_time += time_window * 0.01f;
                    parameter_store.add("scrubber.current_time", current_time);
                }
            }
            else if (mode == ScrubberMode::LATEST)
            {
                event_data->lock_data_vectors();
                const auto &evt_vector = event_data->get_evt_vector_ref(true);
                std::size_t data_size = evt_vector.size();
                event_data->unlock_data_vectors();
                
                if (scrubber_type == ScrubberType::EVENT)
                {
                    if (data_size > 0)
                    {
                        current_index = data_size - 1;
                        parameter_store.add("scrubber.current_index", current_index);
                    }
                }
                else // TIME
                {
                    if (data_size > 0)
                    {
                        // Set to latest timestamp
                        event_data->lock_data_vectors();
                        const auto &evt_vector_abs = event_data->get_evt_vector_ref(false);
                        current_time = evt_vector_abs[data_size - 1].z;
                        event_data->unlock_data_vectors();
                        parameter_store.add("scrubber.current_time", current_time);
                    }
                }
            }
            
            // Get latest values from parameter store
            current_index = parameter_store.get<std::size_t>("scrubber.current_index");
            index_window = parameter_store.get<std::size_t>("scrubber.index_window");
            current_time = parameter_store.get<float>("scrubber.current_time");
            time_window = parameter_store.get<float>("scrubber.time_window");
            
            // Update lower and upper based on current and window
            if (scrubber_type == ScrubberType::EVENT)
            {
                // Calculate window around current_index
                std::size_t half_window = index_window / 2;
                
                if (current_index >= half_window)
                {
                    lower_index = current_index - half_window;
                }
                else
                {
                    lower_index = 0;
                }
                
                upper_index = current_index + half_window;
            }
            else // TIME
            {
                lower_time = current_time - time_window;
                upper_time = current_time + time_window;
            }
        }


        void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        {
            if (!event_data || !copy_pass)
                return;
            
            event_data->lock_data_vectors();
            const auto &evt_vector = event_data->get_evt_vector_ref(true);
            const auto &evt_vector_abs = event_data->get_evt_vector_ref(false);
            const auto &frame_vector = event_data->get_frame_vector_ref(false);
            std::size_t data_size = evt_vector.size();
            event_data->unlock_data_vectors();
            
            if (data_size == 0)
                return;
            
            // Check if data has changed
            bool data_changed = (data_size != last_data_size);
            
            // Determine which data to upload based on scrubber type
            std::size_t lower, upper;
            
            if (scrubber_type == ScrubberType::EVENT)
            {
                lower = std::min(lower_index, data_size);
                upper = std::min(upper_index, data_size);
            }
            else // TIME
            {
                // Find indices based on time range
                lower = 0;
                upper = data_size;
                
                event_data->lock_data_vectors();
                for (std::size_t i = 0; i < data_size; ++i)
                {
                    float timestamp = evt_vector_abs[i].z;
                    if (timestamp < lower_time && lower == 0)
                    {
                        lower = i;
                    }
                    if (timestamp > upper_time)
                    {
                        upper = i;
                        break;
                    }
                }
                event_data->unlock_data_vectors();
            }
            
            // Ensure valid range
            if (upper <= lower || lower >= data_size)
            {
                last_data_size = data_size;
                return;
            }
            
            std::size_t range_size = upper - lower;
            
            // Create or update points buffer
            if (!points_buffer || data_changed || range_size > 0)
            {
                std::vector<glm::vec4> points_data;
                points_data.reserve(range_size);
                
                event_data->lock_data_vectors();
                for (std::size_t i = lower; i < upper; ++i)
                {
                    points_data.push_back(evt_vector[i]);
                }
                event_data->unlock_data_vectors();
                
                if (!points_buffer)
                {
                    SDL_GPUBufferCreateInfo buffer_create_info = {0};
                    buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
                    buffer_create_info.size = std::max(points_data.size() * sizeof(glm::vec4), static_cast<size_t>(1024));
                    points_buffer = SDL_CreateGPUBuffer(gpu_device, &buffer_create_info);
                }
                
                if (!points_data.empty())
                {
                    upload_buffer->upload_to_gpu(copy_pass, points_buffer, points_data.data(), 
                                                 points_data.size() * sizeof(glm::vec4));
                }
            }
            
            last_data_size = data_size;
        }

        std::size_t get_lower_index() const
        {
            return lower_index;
        }

        std::size_t get_current_index() const
        {
            return current_index;
        }

        std::size_t get_upper_index() const
        {
            return upper_index;
        }


};

#endif