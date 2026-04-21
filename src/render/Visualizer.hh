#pragma once
#ifndef VISUALIZER_HH
#define VISUALIZER_HH

#include "util/pch.hh"

#include "render/Camera.hh"
#include "render/RenderTarget.hh"
#include "render/UploadBuffer.hh"

struct DataSource;

/**
 * @brief Renders the 3D event data particle plot into a texture that the GUI displays.
 *
 * The visualizer owns one graphics pipeline per visual element (points, grid, frames, text).
 * A single command buffer per frame does all copies and draws into the data source's
 * RenderTargets color attachment.
 */
class Visualizer
{
    public:
        enum class TIME : uint8_t
        {
            UNIT_S = 0,
            UNIT_MS = 1,
            UNIT_US = 2
        };

        struct Parameters
        {
                float particle_scale = 3.0f;
                glm::vec3 polarity_neg_color = glm::vec3(1.0f, 0.0f, 0.0f);
                glm::vec3 polarity_pos_color = glm::vec3(0.0f, 1.0f, 0.0f);

                TIME unit_type = TIME::UNIT_MS;
                float unit_time_conversion_factor = 1000.0f;

                uint32_t grid_x_subdivisions = 5;
                uint32_t grid_y_subdivisions = 5;
                uint32_t grid_z_subdivisions = 5;
        };

        /**
         * @brief Color + depth attachments the visualizer draws into. The color texture is
         *        sampled by ImGui::Image inside the "3D Visualizer" pane.
         */
        struct RenderTargets
        {
                RenderTarget color;
                RenderTarget depth;

                void init_textures(SDL_GPUDevice *gpu_device, cv::Size resolution);
                void delete_textures(SDL_GPUDevice *gpu_device);
        };

        Visualizer(SDL_GPUDevice *gpu_device);
        ~Visualizer();

        Visualizer(const Visualizer &) = delete;
        Visualizer &operator=(const Visualizer &) = delete;

        /**
         * @brief Render a single frame for the given data source into its color RenderTarget.
         */
        void render(std::shared_ptr<DataSource> data_source);

        void rotate_camera(float x_offset, float y_offset);
        void zoom_camera(float scroll_delta);

    private:
        SDL_GPUDevice *gpu_device = nullptr;
        UploadBuffer upload_buffer;
        Camera camera;

        // One graphics pipeline per visual element
        SDL_GPUGraphicsPipeline *points_pipeline = nullptr;
        SDL_GPUGraphicsPipeline *grid_pipeline = nullptr;
        SDL_GPUGraphicsPipeline *frames_pipeline = nullptr;
        SDL_GPUGraphicsPipeline *text_pipeline = nullptr;

        // Grid geometry is static: built once in the constructor.
        SDL_GPUBuffer *grid_vertex_buffer = nullptr;
        uint32_t grid_vertex_count = 0;

        // Frame texture sampler (2D array, mirrored-repeat).
        SDL_GPUSampler *frames_sampler = nullptr;

        // Text rendering: TTF engine + atlas sampler + growable vertex/index buffers.
        TTF_TextEngine *text_engine = nullptr;
        TTF_Font *text_font = nullptr;
        SDL_GPUSampler *text_sampler = nullptr;
        SDL_GPUBuffer *text_vertex_buffer = nullptr;
        SDL_GPUBuffer *text_index_buffer = nullptr;
        uint32_t text_vertex_buffer_capacity = 0;
        uint32_t text_index_buffer_capacity = 0;
};

#endif // VISUALIZER_HH
