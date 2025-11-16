#pragma once

#include <SDL3/SDL_gpu.h>
#ifndef SCRUBBER_HH
#define SCRUBBER_HH

#include "pch.hh"

#include "EventData.hh"
#include "ParameterStore.hh"
#include "UploadBuffer.hh"

#include <algorithm>
#include <array>

/**
 * @brief Provides functionality for scrubbing through subsets of event data.
 */
class Scrubber
{
    public:
        /**
         * @brief Two types of data scrubbing, event based or time based.
         */
        enum class ScrubberType : std::uint8_t
        {
            EVENT,
            TIME,
        };

        /**
         * @brief Three modes of data scrubbing.
         *        Paused is good for scrubbing through data.
         *        Playing is good for recreating streaming from past data.
         *        Latest is good for seeing data as it is streamed.
         */
        enum class ScrubberMode : std::uint8_t
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
        float lower_depth = 0.0f;
        float upper_depth = 0.0f;
        glm::vec2 camera_resolution = glm::vec2(0.0f, 0.0f);

        SDL_GPUTexture *frames = nullptr;
        std::array<float, 2> frame_timestamps = {-1.0, -1.0};
        std::size_t frame_width = 0, frame_height = 0;

    public:

        /**
         * @brief Constructor. Initializes ParameterStore with necessary variables.
         * @param parameter_store ParameterStore object containing data from GUI
         * @param event_data EventData object containing event/frame data
         * @param gpu_device SDL_GPUDevice to upload event data points to
         */
        Scrubber(ParameterStore &parameter_store, EventData *event_data, SDL_GPUDevice *gpu_device)
            : parameter_store(parameter_store), event_data(event_data), gpu_device(gpu_device)
        {
            parameter_store.add("scrubber.type", ScrubberType::EVENT);
            parameter_store.add("scrubber.mode", ScrubberMode::PAUSED);

            parameter_store.add("scrubber.current_index", current_index);
            parameter_store.add("scrubber.index_window", index_window);
            parameter_store.add("scrubber.index_step", index_step);
            parameter_store.add("scrubber.min_index", static_cast<std::size_t>(0));
            parameter_store.add("scrubber.max_index", static_cast<std::size_t>(0));

            parameter_store.add("scrubber.current_time", current_time);
            parameter_store.add("scrubber.time_window", time_window);
            parameter_store.add("scrubber.time_step", time_step);
            parameter_store.add("scrubber.min_time", 0.0f);
            parameter_store.add("scrubber.max_time", 0.0f);
            parameter_store.add("scrubber.show_frame_data", false);
        }

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
            current_index = parameter_store.get<std::size_t>("scrubber.current_index");
            index_window = parameter_store.get<std::size_t>("scrubber.index_window");
            index_step = parameter_store.get<std::size_t>("scrubber.index_step");

            event_data->lock_data_vectors();

            if (event_data->get_evt_vector_ref().empty())
            {
                event_data->unlock_data_vectors();

                // No event data? Scrubber has nothing to scrub.
                parameter_store.add("scrubber.current_index", 0ULL);
                parameter_store.add("scrubber.index_window", 0ULL);
                parameter_store.add("scrubber.index_step", 0ULL);
                parameter_store.add("scrubber.min_index", 0ULL);
                parameter_store.add("scrubber.max_index", 0ULL);

                parameter_store.add("scrubber.current_time", 0.0f);
                parameter_store.add("scrubber.time_window", 0.0f);
                parameter_store.add("scrubber.time_step", 0.0f);
                parameter_store.add("scrubber.min_time", 0.0f);
                parameter_store.add("scrubber.max_time", 0.0f);
                parameter_store.add("scrubber.show_frame_data", false);

                lower_index = 0;
                current_index = 0;
                index_step = 0;
                index_window = 0;

                lower_time = 0.0f;
                current_time = 0.0f;
                time_step = 0.0f;
                time_window = 0.0f;



                return;
            }

            parameter_store.add("scrubber.min_index", static_cast<std::size_t>(0));
            parameter_store.add("scrubber.max_index", event_data->get_evt_vector_ref().size() - 1);

            if (parameter_store.get<ScrubberType>("scrubber.type") == ScrubberType::TIME)
            {
                current_time = parameter_store.get<float>("scrubber.current_time");
                time_window = parameter_store.get<float>("scrubber.time_window");
                time_step = parameter_store.get<float>("scrubber.time_step");

                // Get time bounds from event data
                const auto &evt_vector_relative = event_data->get_evt_vector_ref();
                float min_time = 0.0f;
                float max_time = 0.0f;
                if (!evt_vector_relative.empty())
                {
                    min_time = 0.0f;                         // Relative timestamps start at 0
                    max_time = evt_vector_relative.back().z; // Last element's timestamp
                }

                parameter_store.add("scrubber.min_time", min_time);
                parameter_store.add("scrubber.max_time", max_time);

                if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::PAUSED)
                {
                    current_time = std::clamp(current_time, min_time, max_time);
                    time_window = std::clamp(time_window, 0.0f, max_time - min_time);
                    time_step = std::clamp(time_step, 0.0f, max_time - min_time);
                    lower_time = std::max(min_time, current_time - time_window);
                }
                else if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::PLAYING)
                {
                    time_step = std::clamp(time_step, 0.0f, max_time - min_time);
                    time_window = std::clamp(time_window, 0.0f, max_time - min_time);
                    current_time = std::clamp(current_time + time_step, min_time, max_time);
                    lower_time = std::max(min_time, current_time - time_window);
                }
                else if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::LATEST)
                {
                    current_time = max_time;
                    time_window = std::clamp(time_window, 0.0f, max_time - min_time);
                    time_step = std::clamp(time_step, 0.0f, max_time - min_time);
                    lower_time = std::max(min_time, current_time - time_window);
                }

                // Convert time values to indices for internal use
                current_index = event_data->get_event_index_from_relative_timestamp(current_time);
                lower_index = event_data->get_event_index_from_relative_timestamp(lower_time);

                // Ensure indices are valid
                if (current_index == -1)
                    current_index = 0;
                if (lower_index == -1)
                    lower_index = 0;
                if (current_index >= evt_vector_relative.size())
                    current_index = evt_vector_relative.size() - 1;
                if (lower_index >= evt_vector_relative.size())
                    lower_index = evt_vector_relative.size() - 1;

                parameter_store.add("scrubber.current_time", current_time);
                parameter_store.add("scrubber.time_window", time_window);
                parameter_store.add("scrubber.time_step", time_step);
                parameter_store.add("scrubber.current_index", current_index);
                parameter_store.add("scrubber.index_window", current_index - lower_index);
                //parameter_store.add("scrubber.index_step", static_cast<std::size_t>(time_step));
            }

            if (parameter_store.get<ScrubberType>("scrubber.type") == ScrubberType::EVENT)
            {
                if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::PAUSED)
                {
                    current_index = std::clamp(current_index, static_cast<std::size_t>(0),
                                               event_data->get_evt_vector_ref().size() - 1);
                    index_window = std::clamp(index_window, static_cast<std::size_t>(0),
                                              event_data->get_evt_vector_ref().size() - 1);
                    index_step = std::clamp(index_step, static_cast<std::size_t>(0),
                                            event_data->get_evt_vector_ref().size() - 1);
                    lower_index = std::max(static_cast<std::size_t>(0), current_index - index_window);
                }
                else if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::PLAYING)
                {
                    index_step = std::clamp(index_step, static_cast<std::size_t>(0),
                                            event_data->get_evt_vector_ref().size() - 1);
                    index_window = std::clamp(index_window, static_cast<std::size_t>(0),
                                              event_data->get_evt_vector_ref().size() - 1);
                    current_index = std::clamp(current_index + index_step, static_cast<std::size_t>(0),
                                               event_data->get_evt_vector_ref().size() - 1);
                    lower_index = std::max(static_cast<std::size_t>(0), current_index - index_window);
                }
                else if (parameter_store.get<ScrubberMode>("scrubber.mode") == ScrubberMode::LATEST)
                {
                    std::size_t event_data_size =
                        event_data->get_evt_vector_ref().empty() ? 0 : event_data->get_evt_vector_ref().size() - 1;
                    current_index = event_data_size;
                    index_window = std::clamp(index_window, static_cast<std::size_t>(0), event_data_size);
                    index_step = std::clamp(index_step, static_cast<std::size_t>(0), event_data_size);
                    lower_index = std::max(static_cast<std::size_t>(0), current_index - index_window);
                }

                parameter_store.add("scrubber.current_index", current_index);
                parameter_store.add("scrubber.index_window", index_window);
                parameter_store.add("scrubber.index_step", index_step);
            }

            event_data->unlock_data_vectors();
        }

        /**
         * @brief Copies relevant event data and frame data into buffer on GPU
         *        to be drawn and processed by DCE.
         * @param upload_buffer UploadBuffer object for uploading data to gpu
         * @param copy_pass SDL_GPUCopyPass for copying data to GPU
         */
        void copy_pass(UploadBuffer *upload_buffer, SDL_GPUCopyPass *copy_pass)
        {
            if (!event_data || !upload_buffer || !copy_pass)
            {
                return;
            }

            event_data->lock_data_vectors();

            // Get the data we need to copy
            const auto &evt_vector = event_data->get_evt_vector_ref();

            // Return if event data is empty
            if (evt_vector.empty())
            {
                event_data->unlock_data_vectors();

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
            num_points = std::min(num_points, evt_vector.size() - lower_index);

            event_data->unlock_data_vectors();

            // If we have no points to upload, skip
            if (num_points == 0)
            {
                return;
            }

            if (lower_index >= evt_vector.size() || current_index >= evt_vector.size())
            {
                return;
            }

            lower_depth = evt_vector[lower_index].z;
            upper_depth = evt_vector[current_index].z;
            camera_resolution = event_data->get_camera_event_resolution();

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

            // Below is frame texture generation code, skip if user does not want frames
            if (!parameter_store.get<bool>("scrubber.show_frame_data"))
            {
                event_data->unlock_data_vectors();

                // To prevent drawing of frames
                frame_timestamps[0] = -1.0f;
                frame_timestamps[1] = -1.0f;
                return;
            }

            // recreate frame if necessary
            glm::vec2 current_frame_dimensions = event_data->get_camera_frame_resolution();
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
            const std::vector<std::pair<cv::Mat, float>> &frame_vector = event_data->get_frame_vector_ref();

            // Initialize both timestamps to -1 (invalid)
            frame_timestamps[0] = -1.0f;
            frame_timestamps[1] = -1.0f;

            if (frame_vector.empty())
            {
                event_data->unlock_data_vectors();
                return;
            }

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
                upload_buffer->upload_cv_mat(copy_pass, frames, frame_vector[frame_idx_0].first, 0);
                frame_timestamps[0] = frame_vector[frame_idx_0].second;

                if (frame_idx_1 != frame_idx_0 && frame_idx_1 < frame_vector.size())
                {
                    upload_buffer->upload_cv_mat(copy_pass, frames, frame_vector[frame_idx_1].first, 1);
                    frame_timestamps[1] = frame_vector[frame_idx_1].second;
                }
                else
                {
                    frame_timestamps[1] = -1.0f;
                }
            }

            event_data->unlock_data_vectors();
        }

        /**
         * @brief Returns the points buffer, 
         *        pointer to buffer in GPU memory containing event data points to be drawn.
         * @return pointer to buffer in GPU memory containing event data points that are inside scrubber window.
         */
        SDL_GPUBuffer *get_points_buffer()
        {
            return points_buffer;
        }

        /**
         * @brief Returns the frames texture.
         * @return pointer to texture in GPU memory containing frames that are inside scrubber window.
         */
        SDL_GPUTexture *get_frames_texture()
        {
            return frames;
        }

        /**
         * @brief Get timestamps of the two frames that are being interpolated in scrubber.
         * @return std::array of timestamps of two frames being interpolated in scrubber.
         */
        std::array<float, 2> get_frames_timestamps()
        {
            return frame_timestamps;
        }

        /**
         * @brief Get dimensions of frames being drawn by scrubber.
         * @return std::array containing width and height of frames being drawn by scrubber.
         */
        std::array<std::size_t, 2> get_frame_dimensions()
        {
            return {frame_width, frame_height};
        }

        /**
         * @brief Returns the size of the points buffer,
         *        size of buffer in GPU memory containing event data as points to be drawn.
         * @return number of elements in the points buffer.
         */ 
        std::size_t get_points_buffer_size()
        {
            return points_buffer_size / sizeof(glm::vec4);
        }

        /**
         * @brief Returns lower time bound of scrubber window.
         * @return lower time bound of scrubber window.
         */
        float get_lower_depth()
        {
            return lower_depth;
        }

        /**
         * @brief Returns upper time bound of scrubber window.
         * @return upper time bound of scrubber window.
         */
        float get_upper_depth()
        {
            return upper_depth;
        }

        /**
         * @brief Gets the event camera resolution.
         * @return event camera resolution.
         */
        glm::vec2 get_camera_resolution()
        {
            return camera_resolution;
        }
};

#endif
