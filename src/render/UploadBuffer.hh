#pragma once
#ifndef UPLOAD_BUFFER_HH
#define UPLOAD_BUFFER_HH

#include "util/pch.hh"

/**
 * @brief Helper for uploading data to the GPU.
 *
 * Holds a single transfer buffer that grows as needed to fit the largest upload.
 */
class UploadBuffer
{
    private:
        SDL_GPUDevice *gpu_device = nullptr;
        SDL_GPUTransferBuffer *transfer_buffer = nullptr;
        Uint32 capacity = 0;

        constexpr static Uint32 initial_capacity = 1 << 20;

        static Uint32 next_pow2(Uint32 v)
        {
            Uint32 r = 1;
            while (r < v)
                r <<= 1;
            return r;
        }

        /**
         * @brief Grows the transfer buffer if the current capacity is too small.
         * @param nbyte Required capacity in bytes.
         */
        void ensure_capacity(size_t nbyte)
        {
            if (nbyte <= capacity)
                return;

            if (transfer_buffer)
                SDL_ReleaseGPUTransferBuffer(gpu_device, transfer_buffer);

            capacity = next_pow2(static_cast<Uint32>(nbyte));
            SDL_GPUTransferBufferCreateInfo info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = capacity};
            transfer_buffer = SDL_CreateGPUTransferBuffer(gpu_device, &info);
        }

    public:
        /**
         * @brief Constructor. Allocates the initial transfer buffer.
         * @param gpu_device SDL_GPUDevice to upload data to.
         */
        UploadBuffer(SDL_GPUDevice *gpu_device) : gpu_device(gpu_device)
        {
            ensure_capacity(initial_capacity);
        }

        /**
         * @brief Destructor. Releases the transfer buffer.
         */
        ~UploadBuffer()
        {
            if (transfer_buffer)
                SDL_ReleaseGPUTransferBuffer(gpu_device, transfer_buffer);
        }

        /**
         * @brief Uploads bytes into an SDL_GPUBuffer in a single copy.
         *
         * Grows the transfer buffer if needed so the full payload fits contiguously;
         * the destination is written starting at offset 0.
         * @param pass Active SDL_GPUCopyPass.
         * @param dst Destination GPU buffer (written from offset 0).
         * @param src Source host pointer.
         * @param nbyte Number of bytes to copy.
         */
        void upload_to_gpu(SDL_GPUCopyPass *pass, SDL_GPUBuffer *dst, const void *src, size_t nbyte)
        {
            if (nbyte == 0)
                return;

            ensure_capacity(nbyte);

            void *ptr = SDL_MapGPUTransferBuffer(gpu_device, transfer_buffer, true);
            SDL_memcpy(ptr, src, nbyte);
            SDL_UnmapGPUTransferBuffer(gpu_device, transfer_buffer);

            SDL_GPUTransferBufferLocation transfer_location = {.transfer_buffer = transfer_buffer};
            SDL_GPUBufferRegion buffer_region = {.buffer = dst, .offset = 0, .size = static_cast<Uint32>(nbyte)};
            SDL_UploadToGPUBuffer(pass, &transfer_location, &buffer_region, false);
        }

        /**
         * @brief Uploads a cv::Mat into one layer of an SDL_GPUTexture.
         *
         * The source Mat is converted from BGR to RGBA before upload. Assumes:
         *   - the destination texture's width/height match the Mat's cols/rows,
         *   - the destination texture format is SDL_GPU_TEXTUREFORMAT_R8G8B8A8_*.
         * The transfer buffer is grown as needed to hold the full converted image.
         * @param pass Active SDL_GPUCopyPass.
         * @param texture Destination GPU texture.
         * @param mat Source image (BGR or BGRA; converted to RGBA internally).
         * @param layer Destination array layer.
         */
        void upload_cv_mat(SDL_GPUCopyPass *pass, SDL_GPUTexture *texture, const cv::Mat &mat, uint32_t layer = 0)
        {
            cv::Mat rgba;
            cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGBA);

            size_t nbyte = rgba.total() * rgba.elemSize();
            ensure_capacity(nbyte);

            void *ptr = SDL_MapGPUTransferBuffer(gpu_device, transfer_buffer, true);
            std::memcpy(ptr, rgba.data, nbyte);
            SDL_UnmapGPUTransferBuffer(gpu_device, transfer_buffer);

            SDL_GPUTextureTransferInfo source_texture_transfer_info = {};
            source_texture_transfer_info.transfer_buffer = transfer_buffer;
            source_texture_transfer_info.offset = 0;
            source_texture_transfer_info.pixels_per_row = rgba.cols;
            source_texture_transfer_info.rows_per_layer = rgba.rows;

            SDL_GPUTextureRegion dest_texture_region = {};
            dest_texture_region.texture = texture;
            dest_texture_region.mip_level = 0;
            dest_texture_region.layer = layer;
            dest_texture_region.x = 0;
            dest_texture_region.y = 0;
            dest_texture_region.z = 0;
            dest_texture_region.w = rgba.cols;
            dest_texture_region.h = rgba.rows;
            dest_texture_region.d = 1;

            SDL_UploadToGPUTexture(pass, &source_texture_transfer_info, &dest_texture_region, false);
        }
};

#endif
