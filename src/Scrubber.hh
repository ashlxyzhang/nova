#pragma once

#ifndef SCRUBBER_HH
#define SCRUBBER_HH

#include "pch.hh"

#include "EventData.hh"
#include "UploadBuffer.hh"

// this class will handle all things to do with scrubbing and window size of particles.
// since at the end of the day, everything is related to values in an vector,
// we need a way to easily switch between time and event domains
class Scrubber 
{
    private:
        EventData *event_data = nullptr;
        // SDL_GPUDevice *gpu_device = nullptr;

        // not sure how to deal with time yet, leave it for later
        // float lower_time = std::numeric_limits<float>::max();
        // float current_time = 0.0f;
        // float upper_time = std::numeric_limits<float>::min();
        // float window_size = 1.0f;

        std::size_t lower_index = std::numeric_limits<std::size_t>::max();
        std::size_t current_index = 0;
        std::size_t upper_index = std::numeric_limits<std::size_t>::min();
        static constexpr std::size_t window_size = 50;

        // SDL_GPUBuffer *points_buffer = nullptr;
        // SDL_GPUTexture *textures = nullptr;
        // SDL_GPUBuffer *textures_timestamps = nullptr;
        

    public:
        Scrubber(EventData *event_data) : event_data(event_data)
        {
            // TODO, set scrub to midpoint, 
            // and lower and upper to min and max to data in event
        }

        ~Scrubber()
        {
        }

        void cpu_update()
        {
            current_index = std::min(current_index + 1, event_data->get_evt_vector_ref(true).size() - 1);
            
            std::size_t max_index = event_data->get_evt_vector_ref(true).size() - 1;
            
            // Handle edge cases for sliding window
            if (current_index == 0) {
                lower_index = 0;
                upper_index = std::min(window_size, max_index);
            } else if (current_index >= max_index) {
                lower_index = (max_index >= window_size) ? (max_index - window_size) : 0;
                upper_index = max_index;
            } else {
                // Normal case: center the window around current_index
                lower_index = std::max(0ULL, current_index - static_cast<std::size_t>(window_size / 2));
                upper_index = std::min(max_index, current_index + window_size / 2);
            }
        }

        // no gpu uploading for now
        // void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        // {
        // }

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