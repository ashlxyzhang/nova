#include "render/Visualizer.hh"
#include "data/DataSource.hh"

#include "shaders/visualizer/frames/frames_frag.h"
#include "shaders/visualizer/frames/frames_vert.h"
#include "shaders/visualizer/grid/grid_frag.h"
#include "shaders/visualizer/grid/grid_vert.h"
#include "shaders/visualizer/points/points_frag.h"
#include "shaders/visualizer/points/points_vert.h"
#include "shaders/visualizer/text/text_frag.h"
#include "shaders/visualizer/text/text_vert.h"

#include "fonts/CascadiaCode.ttf.h"

#include <format>

namespace
{

// UNORM so ImGui::Image samples the full [0, 1] range. SNORM (as in the old code) silently clipped
// all positive color writes to the negative half of the range.
constexpr SDL_GPUTextureFormat kColorFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
constexpr SDL_GPUTextureFormat kDepthFormat = SDL_GPU_TEXTUREFORMAT_D16_UNORM;

// The visualizer is a 3D scene, not a 2D image of the event camera, so it renders into a fixed
// high-resolution surface that the GUI rescales when drawing.
constexpr Uint32 kSurfaceWidth = 1920;
constexpr Uint32 kSurfaceHeight = 1200;

SDL_GPUShader *create_shader(SDL_GPUDevice *gpu_device, const void *code, size_t code_size, SDL_GPUShaderStage stage,
                             Uint32 num_samplers, Uint32 num_uniform_buffers)
{
    SDL_GPUShaderCreateInfo info = {};
    info.code_size = code_size;
    info.code = static_cast<const Uint8 *>(code);
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = stage;
    info.num_samplers = num_samplers;
    info.num_uniform_buffers = num_uniform_buffers;
    return SDL_CreateGPUShader(gpu_device, &info);
}

// Event coordinates (pixels x, pixels y, timestamp z) → unit cube [-1, 1]^3 with Y flipped
// (event camera Y grows downward, 3D world Y grows upward).
glm::mat4 event_to_unit_cube(float res_x, float res_y, float lower_depth, float upper_depth)
{
    float depth_range = upper_depth - lower_depth;
    if (depth_range <= 0.0f) depth_range = 1.0f;
    if (res_x <= 0.0f)       res_x = 1.0f;
    if (res_y <= 0.0f)       res_y = 1.0f;

    glm::mat4 shift_to_origin = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -lower_depth));
    glm::mat4 normalize = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f / res_x, 2.0f / res_y, 2.0f / depth_range));
    glm::mat4 recenter = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, -1.0f, -1.0f));
    glm::mat4 flip_y = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
    return flip_y * recenter * normalize * shift_to_origin;
}

// Lines along the three outside faces (front, bottom, left) of the unit cube. Drawn as GL_LINES.
std::vector<glm::vec3> build_grid_lines(uint32_t sx, uint32_t sy, uint32_t sz)
{
    std::vector<glm::vec3> out;
    auto norm = [](uint32_t i, uint32_t n) { return 2.0f * static_cast<float>(i) / static_cast<float>(n) - 1.0f; };

    // Front face (z = +1)
    for (uint32_t i = 0; i <= sx; ++i) { float x = norm(i, sx); out.emplace_back(x, -1.0f, 1.0f); out.emplace_back(x, 1.0f, 1.0f); }
    for (uint32_t i = 0; i <= sy; ++i) { float y = norm(i, sy); out.emplace_back(-1.0f, y, 1.0f); out.emplace_back(1.0f, y, 1.0f); }
    // Bottom face (y = -1)
    for (uint32_t i = 0; i <= sx; ++i) { float x = norm(i, sx); out.emplace_back(x, -1.0f, -1.0f); out.emplace_back(x, -1.0f, 1.0f); }
    for (uint32_t i = 0; i <= sz; ++i) { float z = norm(i, sz); out.emplace_back(-1.0f, -1.0f, z); out.emplace_back(1.0f, -1.0f, z); }
    // Left face (x = -1)
    for (uint32_t i = 0; i <= sy; ++i) { float y = norm(i, sy); out.emplace_back(-1.0f, y, -1.0f); out.emplace_back(-1.0f, y, 1.0f); }
    for (uint32_t i = 0; i <= sz; ++i) { float z = norm(i, sz); out.emplace_back(-1.0f, -1.0f, z); out.emplace_back(-1.0f, 1.0f, z); }
    return out;
}

// std140 layout for the points vertex shader uniform block. Explicit padding so sizeof matches
// the shader's expected block size (trailing float padded to vec4 alignment).
struct PointsUniforms
{
        glm::mat4 mvp;
        glm::vec4 negative_color;
        glm::vec4 positive_color;
        float point_size;
        float _pad[3];
};

struct TextVertex
{
        glm::vec3 pos;
        SDL_FColor color;
        glm::vec2 uv;
};

struct TextDrawCall
{
        SDL_GPUTexture *atlas;
        Uint32 index_count;
        Uint32 index_offset;
        Sint32 base_vertex;
};

std::string format_timestamp(float timestamp, Visualizer::TIME unit_type, float unit_factor)
{
    float value = timestamp / unit_factor;
    switch (unit_type)
    {
    case Visualizer::TIME::UNIT_US: return std::format("{:.2f}", value);
    case Visualizer::TIME::UNIT_MS: return std::format("{:.4f}", value);
    case Visualizer::TIME::UNIT_S:  return std::format("{:.8f}", value);
    }
    return std::format("{}", value);
}

} // namespace


// ===== RenderTargets =====

void Visualizer::RenderTargets::init_textures(SDL_GPUDevice *gpu_device, cv::Size /*resolution*/)
{
    SDL_GPUTextureCreateInfo color_info = {};
    color_info.type = SDL_GPU_TEXTURETYPE_2D;
    color_info.format = kColorFormat;
    color_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    color_info.width = kSurfaceWidth;
    color_info.height = kSurfaceHeight;
    color_info.layer_count_or_depth = 1;
    color_info.num_levels = 1;
    color_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    color.texture = SDL_CreateGPUTexture(gpu_device, &color_info);
    color.width = kSurfaceWidth;
    color.height = kSurfaceHeight;

    SDL_GPUTextureCreateInfo depth_info = {};
    depth_info.type = SDL_GPU_TEXTURETYPE_2D;
    depth_info.format = kDepthFormat;
    depth_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depth_info.width = kSurfaceWidth;
    depth_info.height = kSurfaceHeight;
    depth_info.layer_count_or_depth = 1;
    depth_info.num_levels = 1;
    depth_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    depth.texture = SDL_CreateGPUTexture(gpu_device, &depth_info);
    depth.width = kSurfaceWidth;
    depth.height = kSurfaceHeight;
}

void Visualizer::RenderTargets::delete_textures(SDL_GPUDevice *gpu_device)
{
    if (color.texture) SDL_ReleaseGPUTexture(gpu_device, color.texture);
    if (depth.texture) SDL_ReleaseGPUTexture(gpu_device, depth.texture);
    color.texture = nullptr;
    depth.texture = nullptr;
}


// ===== Visualizer ctor/dtor =====

Visualizer::Visualizer(SDL_GPUDevice *gpu_device)
    : gpu_device(gpu_device), upload_buffer(gpu_device),
      camera(glm::vec3(0.0f), 4.0f, 45.0f, static_cast<float>(kSurfaceWidth) / static_cast<float>(kSurfaceHeight),
             0.1f, 1000.0f)
{
    // ----- Points pipeline -----
    {
        SDL_GPUShader *vs = create_shader(gpu_device, points_vert, sizeof(points_vert), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
        SDL_GPUShader *fs = create_shader(gpu_device, points_frag, sizeof(points_frag), SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);

        SDL_GPUVertexBufferDescription vbd = {0, sizeof(glm::vec4), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
        SDL_GPUVertexAttribute va = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0};
        SDL_GPUColorTargetDescription color_desc = {.format = kColorFormat};

        SDL_GPUGraphicsPipelineCreateInfo info = {};
        info.vertex_shader = vs;
        info.fragment_shader = fs;
        info.vertex_input_state.vertex_buffer_descriptions = &vbd;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = &va;
        info.vertex_input_state.num_vertex_attributes = 1;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_POINTLIST;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        info.depth_stencil_state.enable_depth_test = true;
        info.depth_stencil_state.enable_depth_write = true;
        info.target_info.color_target_descriptions = &color_desc;
        info.target_info.num_color_targets = 1;
        info.target_info.depth_stencil_format = kDepthFormat;
        info.target_info.has_depth_stencil_target = true;
        points_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &info);

        SDL_ReleaseGPUShader(gpu_device, vs);
        SDL_ReleaseGPUShader(gpu_device, fs);
    }

    // ----- Grid pipeline -----
    {
        SDL_GPUShader *vs = create_shader(gpu_device, grid_vert, sizeof(grid_vert), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
        SDL_GPUShader *fs = create_shader(gpu_device, grid_frag, sizeof(grid_frag), SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);

        SDL_GPUVertexBufferDescription vbd = {0, sizeof(glm::vec3), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
        SDL_GPUVertexAttribute va = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0};
        SDL_GPUColorTargetDescription color_desc = {.format = kColorFormat};

        SDL_GPUGraphicsPipelineCreateInfo info = {};
        info.vertex_shader = vs;
        info.fragment_shader = fs;
        info.vertex_input_state.vertex_buffer_descriptions = &vbd;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = &va;
        info.vertex_input_state.num_vertex_attributes = 1;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        info.depth_stencil_state.enable_depth_test = true;
        info.depth_stencil_state.enable_depth_write = true;
        info.target_info.color_target_descriptions = &color_desc;
        info.target_info.num_color_targets = 1;
        info.target_info.depth_stencil_format = kDepthFormat;
        info.target_info.has_depth_stencil_target = true;
        grid_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &info);

        SDL_ReleaseGPUShader(gpu_device, vs);
        SDL_ReleaseGPUShader(gpu_device, fs);
    }

    // Build static grid vertex buffer once.
    {
        Parameters defaults;
        auto lines = build_grid_lines(defaults.grid_x_subdivisions, defaults.grid_y_subdivisions,
                                      defaults.grid_z_subdivisions);
        grid_vertex_count = static_cast<uint32_t>(lines.size());

        SDL_GPUBufferCreateInfo bci = {};
        bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bci.size = static_cast<Uint32>(lines.size() * sizeof(glm::vec3));
        grid_vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &bci);

        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu_device);
        SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
        upload_buffer.upload_to_gpu(cp, grid_vertex_buffer, lines.data(), lines.size() * sizeof(glm::vec3));
        SDL_EndGPUCopyPass(cp);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_WaitForGPUIdle(gpu_device);
    }

    // ----- Frames pipeline + sampler -----
    {
        SDL_GPUShader *vs = create_shader(gpu_device, frames_vert, sizeof(frames_vert), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
        SDL_GPUShader *fs = create_shader(gpu_device, frames_frag, sizeof(frames_frag), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);

        SDL_GPUColorTargetDescription color_desc = {.format = kColorFormat};

        SDL_GPUGraphicsPipelineCreateInfo info = {};
        info.vertex_shader = vs;
        info.fragment_shader = fs;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        info.depth_stencil_state.enable_depth_test = true;
        info.depth_stencil_state.enable_depth_write = true;
        info.target_info.color_target_descriptions = &color_desc;
        info.target_info.num_color_targets = 1;
        info.target_info.depth_stencil_format = kDepthFormat;
        info.target_info.has_depth_stencil_target = true;
        frames_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &info);

        SDL_ReleaseGPUShader(gpu_device, vs);
        SDL_ReleaseGPUShader(gpu_device, fs);

        SDL_GPUSamplerCreateInfo sampler_info = {};
        sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
        sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
        sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
        sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
        sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        frames_sampler = SDL_CreateGPUSampler(gpu_device, &sampler_info);
    }

    // ----- Text pipeline + font/engine/sampler -----
    {
        TTF_Init(); // Safe to call multiple times; refcounted.
        text_engine = TTF_CreateGPUTextEngine(gpu_device);
        SDL_IOStream *io = SDL_IOFromConstMem(CascadiaCode_ttf, sizeof(CascadiaCode_ttf));
        text_font = TTF_OpenFontIO(io, true, 24.0f);

        SDL_GPUShader *vs = create_shader(gpu_device, text_vert, sizeof(text_vert), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
        SDL_GPUShader *fs = create_shader(gpu_device, text_frag, sizeof(text_frag), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);

        SDL_GPUVertexAttribute vas[3] = {
            {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(TextVertex, pos)},
            {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(TextVertex, color)},
            {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(TextVertex, uv)},
        };
        SDL_GPUVertexBufferDescription vbd = {0, sizeof(TextVertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};

        SDL_GPUColorTargetDescription color_desc = {};
        color_desc.format = kColorFormat;
        color_desc.blend_state.enable_blend = true;
        color_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        color_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        color_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        color_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        color_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        color_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        color_desc.blend_state.color_write_mask = 0xF;

        SDL_GPUGraphicsPipelineCreateInfo info = {};
        info.vertex_shader = vs;
        info.fragment_shader = fs;
        info.vertex_input_state.vertex_buffer_descriptions = &vbd;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = vas;
        info.vertex_input_state.num_vertex_attributes = 3;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        info.depth_stencil_state.enable_depth_test = true;
        info.depth_stencil_state.enable_depth_write = true;
        info.target_info.color_target_descriptions = &color_desc;
        info.target_info.num_color_targets = 1;
        info.target_info.depth_stencil_format = kDepthFormat;
        info.target_info.has_depth_stencil_target = true;
        text_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &info);

        SDL_ReleaseGPUShader(gpu_device, vs);
        SDL_ReleaseGPUShader(gpu_device, fs);

        SDL_GPUSamplerCreateInfo sampler_info = {};
        sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
        sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
        sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        text_sampler = SDL_CreateGPUSampler(gpu_device, &sampler_info);
    }
}

Visualizer::~Visualizer()
{
    if (!gpu_device) return;
    SDL_WaitForGPUIdle(gpu_device);

    if (text_vertex_buffer) SDL_ReleaseGPUBuffer(gpu_device, text_vertex_buffer);
    if (text_index_buffer)  SDL_ReleaseGPUBuffer(gpu_device, text_index_buffer);
    if (text_sampler)       SDL_ReleaseGPUSampler(gpu_device, text_sampler);
    if (text_font)          TTF_CloseFont(text_font);
    if (text_engine)        TTF_DestroyGPUTextEngine(text_engine);

    if (frames_sampler)     SDL_ReleaseGPUSampler(gpu_device, frames_sampler);

    if (grid_vertex_buffer) SDL_ReleaseGPUBuffer(gpu_device, grid_vertex_buffer);

    if (points_pipeline) SDL_ReleaseGPUGraphicsPipeline(gpu_device, points_pipeline);
    if (grid_pipeline)   SDL_ReleaseGPUGraphicsPipeline(gpu_device, grid_pipeline);
    if (frames_pipeline) SDL_ReleaseGPUGraphicsPipeline(gpu_device, frames_pipeline);
    if (text_pipeline)   SDL_ReleaseGPUGraphicsPipeline(gpu_device, text_pipeline);
}


// ===== Camera control =====

void Visualizer::rotate_camera(float x_offset, float y_offset)
{
    camera.processMouseMovement(x_offset, y_offset);
}

void Visualizer::zoom_camera(float scroll_delta)
{
    camera.processMouseScroll(scroll_delta);
}


// ===== Render =====

void Visualizer::render(std::shared_ptr<DataSource> data_source)
{
    const Parameters &params = data_source->visualizer_parameters;
    RenderTargets &render_targets = data_source->visualizer_render_targets;
    Scrubber &scrubber = data_source->scrubber;

    if (!render_targets.color.texture || !render_targets.depth.texture) return;

    // ----- Build text geometry on the CPU -----
    std::vector<TextVertex> text_vertices;
    std::vector<Uint32> text_indices;
    std::vector<TextDrawCall> text_draw_calls;
    std::vector<TTF_Text *> owned_text;

    if (text_engine && text_font)
    {
        float lower_depth = scrubber.get_lower_depth();
        float upper_depth = scrubber.get_upper_depth();
        float depth_range = upper_depth - lower_depth;

        if (depth_range > 0.0f)
        {
            const SDL_FColor black{0.0f, 0.0f, 0.0f, 1.0f};
            const float pixel_to_world = 0.0025f;
            // Text plane: laid out along world +Z (text local x) and world +Y (text local y).
            const glm::vec3 right(0.0f, 0.0f, 1.0f);
            const glm::vec3 up(0.0f, 1.0f, 0.0f);

            for (uint32_t i = 0; i <= params.grid_z_subdivisions; ++i)
            {
                float z_norm = 2.0f * static_cast<float>(i) / static_cast<float>(params.grid_z_subdivisions) - 1.0f;
                float timestamp = lower_depth + (z_norm + 1.0f) * 0.5f * depth_range;
                std::string label = format_timestamp(timestamp, params.unit_type, params.unit_time_conversion_factor);

                TTF_Text *text_obj = TTF_CreateText(text_engine, text_font, label.c_str(), 0);
                if (!text_obj) continue;
                owned_text.push_back(text_obj);

                TTF_GPUAtlasDrawSequence *sequence = TTF_GetGPUTextDrawData(text_obj);
                if (!sequence) continue;

                glm::vec3 position(1.0f, -1.0f, z_norm);

                for (TTF_GPUAtlasDrawSequence *seq = sequence; seq != nullptr; seq = seq->next)
                {
                    Uint32 index_offset = static_cast<Uint32>(text_indices.size());
                    Sint32 base_vertex = static_cast<Sint32>(text_vertices.size());

                    for (int v = 0; v < seq->num_vertices; ++v)
                    {
                        float lx = seq->xy[v].x * pixel_to_world;
                        float ly = -seq->xy[v].y * pixel_to_world; // TTF Y grows down, flip to world up
                        glm::vec3 world = position + right * lx + up * ly;
                        text_vertices.push_back({world, black, glm::vec2(seq->uv[v].x, seq->uv[v].y)});
                    }
                    for (int k = 0; k < seq->num_indices; ++k)
                        text_indices.push_back(static_cast<Uint32>(seq->indices[k]));

                    text_draw_calls.push_back({seq->atlas_texture, static_cast<Uint32>(seq->num_indices),
                                               index_offset, base_vertex});
                }
            }
        }
    }

    // ----- Acquire command buffer -----
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu_device);

    // ----- Copy pass: upload text vertex + index data.
    // UploadBuffer maps with cycle=true, so each call rotates to a fresh internal transfer
    // resource — safe to do two uploads in the same copy pass.
    if (!text_vertices.empty() && !text_indices.empty())
    {
        Uint32 vbytes = static_cast<Uint32>(text_vertices.size() * sizeof(TextVertex));
        Uint32 ibytes = static_cast<Uint32>(text_indices.size() * sizeof(Uint32));

        if (text_vertex_buffer_capacity < vbytes)
        {
            if (text_vertex_buffer) SDL_ReleaseGPUBuffer(gpu_device, text_vertex_buffer);
            text_vertex_buffer_capacity = std::max(vbytes, text_vertex_buffer_capacity * 2u);
            SDL_GPUBufferCreateInfo bci = {.usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = text_vertex_buffer_capacity};
            text_vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &bci);
        }
        if (text_index_buffer_capacity < ibytes)
        {
            if (text_index_buffer) SDL_ReleaseGPUBuffer(gpu_device, text_index_buffer);
            text_index_buffer_capacity = std::max(ibytes, text_index_buffer_capacity * 2u);
            SDL_GPUBufferCreateInfo bci = {.usage = SDL_GPU_BUFFERUSAGE_INDEX, .size = text_index_buffer_capacity};
            text_index_buffer = SDL_CreateGPUBuffer(gpu_device, &bci);
        }

        SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
        upload_buffer.upload_to_gpu(cp, text_vertex_buffer, text_vertices.data(), vbytes);
        upload_buffer.upload_to_gpu(cp, text_index_buffer, text_indices.data(), ibytes);
        SDL_EndGPUCopyPass(cp);
    }

    // ----- Render pass -----
    SDL_GPUColorTargetInfo color_info = {};
    color_info.texture = render_targets.color.texture;
    color_info.clear_color = SDL_FColor{1.0f, 1.0f, 1.0f, 1.0f};
    color_info.load_op = SDL_GPU_LOADOP_CLEAR;
    color_info.store_op = SDL_GPU_STOREOP_STORE;
    color_info.mip_level = 0;
    color_info.layer_or_depth_plane = 0;
    color_info.cycle = false;

    SDL_GPUDepthStencilTargetInfo depth_info = {};
    depth_info.texture = render_targets.depth.texture;
    depth_info.clear_depth = 1.0f;
    depth_info.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth_info.cycle = false;

    SDL_GPURenderPass *rp = SDL_BeginGPURenderPass(cmd, &color_info, 1, &depth_info);

    camera.setAspectRatio(static_cast<float>(render_targets.color.width) /
                          static_cast<float>(render_targets.color.height));
    glm::mat4 vp = camera.getViewProjectionMatrix();

    // --- Grid ---
    if (grid_pipeline && grid_vertex_buffer && grid_vertex_count > 0)
    {
        SDL_BindGPUGraphicsPipeline(rp, grid_pipeline);
        SDL_GPUBufferBinding vb = {grid_vertex_buffer, 0};
        SDL_BindGPUVertexBuffers(rp, 0, &vb, 1);
        SDL_PushGPUVertexUniformData(cmd, 0, &vp, sizeof(vp));
        SDL_DrawGPUPrimitives(rp, grid_vertex_count, 1, 0, 0);
    }

    // --- Frames ---
    auto frame_timestamps = scrubber.get_frames_timestamps();
    SDL_GPUTexture *frames_texture = scrubber.get_frames_texture();
    if (frames_pipeline && frames_texture && frame_timestamps[0] >= 0.0f)
    {
        SDL_BindGPUGraphicsPipeline(rp, frames_pipeline);
        // Rotate the quad 180° around Z so the frame is oriented correctly relative to event coords.
        glm::mat4 frames_model = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 frames_mvp = vp * frames_model;
        SDL_PushGPUVertexUniformData(cmd, 0, &frames_mvp, sizeof(frames_mvp));

        SDL_GPUTextureSamplerBinding tb{frames_texture, frames_sampler};
        SDL_BindGPUFragmentSamplers(rp, 0, &tb, 1);

        glm::vec4 frame_data(frame_timestamps[0], frame_timestamps[1], scrubber.get_upper_depth(), 0.0f);
        SDL_PushGPUFragmentUniformData(cmd, 0, &frame_data, sizeof(frame_data));

        SDL_DrawGPUPrimitives(rp, 6, 1, 0, 0);
    }

    // --- Points ---
    SDL_GPUBuffer *points_buffer = scrubber.get_points_buffer();
    std::size_t num_points = scrubber.get_points_buffer_size();
    if (points_pipeline && points_buffer && num_points > 0)
    {
        SDL_BindGPUGraphicsPipeline(rp, points_pipeline);
        SDL_GPUBufferBinding vb = {points_buffer, 0};
        SDL_BindGPUVertexBuffers(rp, 0, &vb, 1);

        glm::vec2 res = scrubber.get_camera_resolution();
        glm::mat4 model = event_to_unit_cube(res.x, res.y, scrubber.get_lower_depth(), scrubber.get_upper_depth());

        PointsUniforms u{};
        u.mvp = vp * model;
        u.negative_color = glm::vec4(params.polarity_neg_color, 1.0f);
        u.positive_color = glm::vec4(params.polarity_pos_color, 1.0f);
        u.point_size = params.particle_scale;
        SDL_PushGPUVertexUniformData(cmd, 0, &u, sizeof(u));

        SDL_DrawGPUPrimitives(rp, static_cast<Uint32>(num_points), 1, 0, 0);
    }

    // --- Text ---
    if (text_pipeline && !text_draw_calls.empty() && text_vertex_buffer && text_index_buffer)
    {
        SDL_BindGPUGraphicsPipeline(rp, text_pipeline);
        SDL_GPUBufferBinding vb{text_vertex_buffer, 0};
        SDL_GPUBufferBinding ib{text_index_buffer, 0};
        SDL_BindGPUVertexBuffers(rp, 0, &vb, 1);
        SDL_BindGPUIndexBuffer(rp, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_PushGPUVertexUniformData(cmd, 0, &vp, sizeof(vp));

        for (const auto &call : text_draw_calls)
        {
            SDL_GPUTextureSamplerBinding tb{call.atlas, text_sampler};
            SDL_BindGPUFragmentSamplers(rp, 0, &tb, 1);
            SDL_DrawGPUIndexedPrimitives(rp, call.index_count, 1, call.index_offset, call.base_vertex, 0);
        }
    }

    SDL_EndGPURenderPass(rp);
    SDL_SubmitGPUCommandBuffer(cmd);

    for (TTF_Text *t : owned_text) TTF_DestroyText(t);

    // The scrubber releases its points_buffer on the next update() before rendering the next
    // frame, so wait for the GPU to finish this frame before returning. Mirrors DCE's pattern.
    SDL_WaitForGPUIdle(gpu_device);
}
