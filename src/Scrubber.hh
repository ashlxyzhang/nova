#pragma once

#include <SDL3/SDL_gpu.h>
#ifndef SCRUBBER_HH
#define SCRUBBER_HH

#include "pch.hh"

#include "EventData.hh"
#include "UploadBuffer.hh"
#include "ErrorQueue.hh"

#include <algorithm>
#include <array>
#include <mutex>
#include <shared_mutex>
#include <iostream>

// so the idea is that we give the user two options for scrubbing through the data,
// an event and a time based system. however at the end of the day
// everything is an index into an array, so we need to keep all these values in
// sync in this class
// since we are looking for streaming data support, lets make the window always face behind the
// current stamp
// - Phase 2 



/**
 * @brief Provides functionality for scrubbing through subsets of event data.
 */
class Scrubber
{
    public:

        /**
         * @brief Two types of data scrubbing, event based or time based.
         */
        enum class ScrubberType : std::uint8_t { EVENT, TIME };

        /**
         * @brief Three modes of data scrubbing.
         *        Paused is good for scrubbing through data.
         *        Playing is good for recreating streaming from past data.
         *        Latest is good for seeing data as it is streamed.
         */
        enum class ScrubberMode : std::uint8_t { PAUSED, PLAYING, LATEST };

        struct ScrubberState {
            ScrubberType type = ScrubberType::EVENT;
            ScrubberMode mode = ScrubberMode::PAUSED;

            std::size_t current_index = 0;
            std::size_t index_window = 0;
            std::size_t index_step = 0;
            std::size_t min_index = 0;
            std::size_t max_index = 0;
            std::size_t lower_index = 0;

            float lower_time = 0.0f;
            float current_time = 0.0f;
            float time_window = 0.0f;
            float time_step = 0.0f;
            float min_time = 0.0f;
            float max_time = 0.0f;

            bool show_frame_data = false;

            // Resets every value except for TYPE and MODE
            void clear() {
                current_index = 0;
                index_window = 0;
                index_step = 0;
                min_index = 0;
                max_index = 0;
                lower_index = 0;

                lower_time = 0.0f;
                current_time = 0.0f;
                time_window = 0.0f;
                time_step = 0.0f;
                min_time = 0.0f;
                max_time = 0.0f;

                show_frame_data = false;
            }

            // Updates based on event_data
            void update(EventData& event_data) {
                event_data.lock_data_vectors();
                const auto &event_vector = event_data.get_evt_vector_ref();

                // If no event data, scrubber has nothing to scrub so clear state 
                if (event_vector.empty())
                {   
                    clear();
                }
                else {

                    // Get size of event data 
                    min_index = 0;
                    max_index = event_vector.size() - 1;

                    // Time-based updates
                    if (type == ScrubberType::TIME)
                    {

                        // Get time bounds from event data
                        min_time = 0.0f;
                        max_time = event_vector.empty() ? 0.0f : event_vector.back().z;

                        // Update based on mode
                        if (mode == ScrubberMode::PAUSED)
                        {
                            current_time = std::clamp(current_time, min_time, max_time);
                            time_window = std::clamp(time_window, 0.0f, max_time - min_time);
                            time_step = std::clamp(time_step, 0.0f, max_time - min_time);
                            lower_time = (std::max)(min_time, current_time - time_window);
                        }
                        else if (mode == ScrubberMode::PLAYING)
                        {
                            time_step = std::clamp(time_step, 0.0f, max_time - min_time);
                            time_window = std::clamp(time_window, 0.0f, max_time - min_time);
                            current_time = std::clamp(current_time + time_step, min_time, max_time);
                            lower_time = (std::max)(min_time, current_time - time_window);
                        }
                        else if (mode == ScrubberMode::LATEST)
                        {
                            current_time = max_time;
                            time_window = std::clamp(time_window, 0.0f, max_time - min_time);
                            time_step = std::clamp(time_step, 0.0f, max_time - min_time);
                            lower_time = (std::max)(min_time, current_time - time_window);
                        }


                        // Convert time values to indices for internal use
                        lower_index = event_data.get_event_index_from_relative_timestamp(lower_time);
                        current_index = event_data.get_event_index_from_relative_timestamp(current_time);

                        // Clamp indices
                        lower_index = std::clamp(lower_index, size_t(0), event_vector.size() - 1);
                        current_index = std::clamp(current_index, size_t(0), event_vector.size() - 1);
                    }

                    // Event-based Updates
                    if (type == ScrubberType::EVENT)
                    {   
                        // Update based on mode
                        if (mode == ScrubberMode::PAUSED)
                        {
                            current_index = std::clamp(current_index, size_t(0), event_vector.size() - 1);
                            index_window = std::clamp(index_window, size_t(0), event_vector.size() - 1);
                            index_step = std::clamp(index_step, size_t(0), event_vector.size() - 1);
                            lower_index = (std::max)(size_t(0), current_index - index_window);
                        }
                        else if (mode == ScrubberMode::PLAYING)
                        {
                            index_step = std::clamp(index_step, size_t(0), event_vector.size() - 1);
                            index_window = std::clamp(index_window, size_t(0), event_vector.size() - 1);
                            current_index = std::clamp(current_index + index_step, size_t(0), event_vector.size() - 1);
                            lower_index = (std::max)(size_t(0), current_index - index_window);
                        }
                        else if (mode == ScrubberMode::LATEST)
                        {
                            std::size_t event_data_size = event_vector.empty() ? 0 : event_vector.size() - 1;
                            current_index = event_data_size;
                            index_window = std::clamp(index_window, size_t(0), event_data_size);
                            index_step = std::clamp(index_step, size_t(0), event_data_size);
                            lower_index = (std::max)(size_t(0), current_index - index_window);
                        }
                    }
                }

                event_data.unlock_data_vectors();
            }
        };
    
    
    private:
        mutable std::shared_mutex mutex; // Used by getters, setters, and internal methods performing bulk atomic operations

        // State
        ScrubberState state;
        // -----

    
        // Parameters
        bool show_frame_data = false;
        // -----
        

        // Modules
        EventData &event_data;
        ErrorQueue &error_queue;
        // -----


        // GPU
        SDL_GPUDevice* gpu_device;

        SDL_GPUBuffer *points_buffer = nullptr;
        std::size_t points_buffer_size = 0;
        float lower_depth = 0.0f;
        float upper_depth = 0.0f;
        glm::vec2 camera_resolution = glm::vec2(0.0f, 0.0f);

        SDL_GPUTexture *frames = nullptr;
        std::array<float, 2> frame_timestamps = {-1.0, -1.0};
        std::size_t frame_width = 0, frame_height = 0;
        // -----


    public:
        /**
         * @brief Constructor. Initializes ParameterStore with necessary variables.
         * @param parameter_store ParameterStore object containing data from GUI
         * @param event_data EventData object containing event/frame data
         * @param gpu_device SDL_GPUDevice to upload event data points to
         * @param error_queue ErrorQueue object used to report errors to be displayed and/or logged
         */
        Scrubber(EventData &event_data, SDL_GPUDevice *gpu_device, ErrorQueue &error_queue): 
                event_data(event_data), gpu_device(gpu_device), error_queue(error_queue) {};

        /**
         * @brief Destructor. Releases event points buffer.
         */
        ~Scrubber()
        {
            if (points_buffer)
            {
                SDL_ReleaseGPUBuffer(gpu_device, points_buffer);
            }
            if (frames)
            {
                SDL_ReleaseGPUTexture(gpu_device, frames);
            }
        }

        /**
         * @brief Updates what event data is being captured by scrubber every frame.
         */
        void cpu_update()
        {     
            // Make copy of state to prevent changes from being made mid function
            ScrubberState cur_state = get_state();

            // Update this intermediate state based on event-data
            cur_state.update(event_data);

            // Save changes made to state (may potentially override changes from the GUI)
            set_state(cur_state);
        }


        /**
         * @brief Copies relevant event data and frame data into buffer on GPU
         *        to be drawn and processed by DCE.
         * @param upload_buffer UploadBuffer object for uploading data to gpu
         * @param copy_pass SDL_GPUCopyPass for copying data to GPU
         */
        void copy_pass(UploadBuffer &upload_buffer, SDL_GPUCopyPass *copy_pass)
        {
            if (!copy_pass) return;

            // Lock resources
            event_data.lock_data_vectors();

            // Read indices from state
            ScrubberState scrubber_state = get_state();
            std::size_t current_index = scrubber_state.current_index;
            std::size_t lower_index = scrubber_state.lower_index;

            // Get reference to data we need to copy
            const auto &evt_vector = event_data.get_evt_vector_ref();

            // Return if event data is empty
            if (evt_vector.empty())
            {
                event_data.unlock_data_vectors();

                // Delete old buffer if it exists
                if (points_buffer)
                {
                    SDL_ReleaseGPUBuffer(gpu_device, points_buffer);
                    points_buffer = nullptr;
                }

                // Nothing to draw
                points_buffer_size = 0;

                // To prevent drawing of frames
                frame_timestamps[0] = -1.0f;
                frame_timestamps[1] = -1.0f;
                return;
            }

            // Calculate how many points we need to upload (from lower_index to current_index)
            std::size_t num_points = 0;
            if (current_index >= lower_index)
            {
                num_points = current_index - lower_index + 1;
            }

            // Clamp to the actual size of the vector
            num_points = (std::min)(num_points, evt_vector.size() - lower_index);

            // If we have no points to upload, skip
            if (num_points == 0)
            {
                event_data.unlock_data_vectors();
                return;
            }

            if (lower_index >= evt_vector.size() || current_index >= evt_vector.size())
            {
                event_data.unlock_data_vectors();
                return;
            }

            lower_depth = evt_vector[lower_index].z;
            upper_depth = evt_vector[current_index].z;
            camera_resolution = event_data.get_camera_event_resolution();

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


            // Unlock mutex while uploading the GPU 
            const glm::vec4 *data_ptr = evt_vector.data() + lower_index;
            upload_buffer.upload_to_gpu(copy_pass, points_buffer, data_ptr, points_buffer_size);


            // Below is frame texture generation code, skip if user does not want frames
            const std::vector<std::pair<cv::Mat, float>> &frame_vector = event_data.get_frame_vector_ref();
            glm::vec2 current_frame_dimensions = event_data.get_camera_frame_resolution();

            // Also sanity check the dimensions of the frame
            if (!show_frame_data || frame_vector.empty() ||
                (current_frame_dimensions.x < 1.0f || current_frame_dimensions.y < 1.0f))
            {
                event_data.unlock_data_vectors();

                // To prevent drawing of frames
                frame_timestamps[0] = -1.0f;
                frame_timestamps[1] = -1.0f;
                return;
            }

            // recreate frame if necessary
            if (frame_width != current_frame_dimensions.x || frame_height != current_frame_dimensions.y)
            {
                if (frames != nullptr)
                {
                    SDL_ReleaseGPUTexture(gpu_device, frames);
                    frames = nullptr;
                }
                frame_width = current_frame_dimensions.x;
                frame_height = current_frame_dimensions.y;

                SDL_GPUTextureCreateInfo frames_create_info = {};
                frames_create_info.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
                frames_create_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
                frames_create_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
                frames_create_info.width = frame_width;
                frames_create_info.height = frame_height;
                frames_create_info.layer_count_or_depth = 2;
                frames_create_info.num_levels = 1;
                frames = SDL_CreateGPUTexture(gpu_device, &frames_create_info);
            }

            // Find frames within the time window [lower_depth, upper_depth]
            // and determine which 2 frames should be used for interpolation
            // If there's only one in the window, use that but set the second timestamp to -1
            // If there are no frames in the window, make both -1
            // Initialize both timestamps to -1 (invalid)

            frame_timestamps[0] = -1.0f;
            frame_timestamps[1] = -1.0f;

            // Since frame_vector is sorted by timestamp, use binary search to find frames
            // that bracket upper_depth (the current time we're interpolating at)
            std::pair<cv::Mat, float> target_pair{cv::Mat{}, upper_depth};
            auto lb = std::lower_bound(frame_vector.begin(), frame_vector.end(), target_pair, frame_less_vec4_t);

            std::size_t frame_idx_0 = 0;
            std::size_t frame_idx_1 = 0;
            bool found_valid_frames = false;

            // Determine which two frames to use for interpolation
            if (lb == frame_vector.end())
            {
                // upper_depth is after all frames - use the last two frames
                if (frame_vector.size() >= 2)
                {
                    frame_idx_0 = frame_vector.size() - 2;
                    frame_idx_1 = frame_vector.size() - 1;
                    found_valid_frames = true;
                }
                else if (frame_vector.size() == 1)
                {
                    frame_idx_0 = 0;
                    found_valid_frames = true;
                }
            }
            else if (lb == frame_vector.begin())
            {
                // upper_depth is before all frames - use the first two frames
                if (frame_vector.size() >= 2)
                {
                    frame_idx_0 = 0;
                    frame_idx_1 = 1;
                    found_valid_frames = true;
                }
                else if (frame_vector.size() == 1)
                {
                    frame_idx_0 = 0;
                    found_valid_frames = true;
                }
            }
            else
            {
                // upper_depth is between frames - find the frame before and after
                std::size_t after_idx = std::distance(frame_vector.begin(), lb);
                std::size_t before_idx = after_idx - 1;

                // Check if frames are within the time window
                bool before_in_window =
                    (frame_vector[before_idx].second >= lower_depth && frame_vector[before_idx].second <= upper_depth);
                bool after_in_window =
                    (frame_vector[after_idx].second >= lower_depth && frame_vector[after_idx].second <= upper_depth);

                if (before_in_window && after_in_window)
                {
                    // Both frames are in window - use them
                    frame_idx_0 = before_idx;
                    frame_idx_1 = after_idx;
                    found_valid_frames = true;
                }
                else if (before_in_window)
                {
                    // Only before frame is in window
                    frame_idx_0 = before_idx;
                    found_valid_frames = true;
                }
                else if (after_in_window)
                {
                    // Only after frame is in window
                    frame_idx_0 = after_idx;
                    found_valid_frames = true;
                }
                else
                {
                    // Neither frame is in window, but use them anyway for interpolation
                    frame_idx_0 = before_idx;
                    frame_idx_1 = after_idx;
                    found_valid_frames = true;
                }
            }

            // Upload frames and set timestamps
            if (found_valid_frames)
            {
                upload_buffer.upload_cv_mat(copy_pass, frames, frame_vector[frame_idx_0].first, 0);
                frame_timestamps[0] = frame_vector[frame_idx_0].second;

                if (frame_idx_1 != frame_idx_0 && frame_idx_1 < frame_vector.size())
                {
                    upload_buffer.upload_cv_mat(copy_pass, frames, frame_vector[frame_idx_1].first, 1);
                    frame_timestamps[1] = frame_vector[frame_idx_1].second;
                }
                else
                {
                    frame_timestamps[1] = -1.0f;
                }
            }

            event_data.unlock_data_vectors();
        }

        // Thread safe state getter
        ScrubberState get_state() const {
            std::shared_lock lock(mutex);
            return state;
        }

        // Thread safe state setter
        void set_state(const ScrubberState& new_state) {
            std::unique_lock lock(mutex);
            state = new_state;
        }
    
        // Non thread-safe getters
        SDL_GPUBuffer* get_points_buffer() const { return points_buffer; }
        SDL_GPUTexture* get_frames_texture() const { return frames; }
        std::array<float, 2> get_frames_timestamps() const { return frame_timestamps; }
        std::array<std::size_t, 2> get_frame_dimensions() const { return {frame_width, frame_height}; }
        std::size_t get_points_buffer_size() const { return points_buffer_size / sizeof(glm::vec4); }
        float get_lower_depth() const { return lower_depth; }
        float get_upper_depth() const { return upper_depth; }
        glm::vec2 get_camera_resolution() const { return camera_resolution; }
};

#endif
