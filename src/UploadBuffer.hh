#pragma once

#ifndef UPLOAD_BUFFER_HH
#define UPLOAD_BUFFER_HH

#include "pch.hh"

class UploadBuffer
{
    private:
        SDL_GPUDevice *gpu_device = nullptr;
        SDL_GPUTransferBuffer *transfer_buffer = nullptr;

        constexpr static unsigned buffer_size = 1 << 20;

    public:
        UploadBuffer(SDL_GPUDevice *gpu_device) : gpu_device(gpu_device)
        {
            SDL_GPUTransferBufferCreateInfo transfer_buffer_create_info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                                           .size = buffer_size};
            transfer_buffer = SDL_CreateGPUTransferBuffer(gpu_device, &transfer_buffer_create_info);
        }

        ~UploadBuffer()
        {
            SDL_ReleaseGPUTransferBuffer(gpu_device, transfer_buffer);
        }

        void upload_to_gpu(SDL_GPUCopyPass *pass, SDL_GPUBuffer *dst, const void *src, size_t nbyte)
        {
            size_t offset = 0;
            while (nbyte > 0)
            {
                size_t num = nbyte;
                if (num > buffer_size)
                    num = buffer_size;

                void *ptr = SDL_MapGPUTransferBuffer(gpu_device, transfer_buffer, true);
                SDL_memcpy(ptr, (const char *)src + offset, num);
                SDL_UnmapGPUTransferBuffer(gpu_device, transfer_buffer);

                SDL_GPUTransferBufferLocation transfer_location = {.transfer_buffer = transfer_buffer};
                SDL_GPUBufferRegion buffer_region = {
                    .buffer = dst, .offset = static_cast<Uint32>(offset), .size = static_cast<Uint32>(num)};

                SDL_UploadToGPUBuffer(pass, &transfer_location, &buffer_region, false);

                nbyte -= num;
                offset += num;
            }
        }

        // This function will make a lot of assumptions
        // first is that the SDL_GPUTexture is the same resolution as the cv::Mat
        // second is that they have the same number of bits per channel
        // third is that the texture was created with rgba8 unorm format
        void upload_cv_mat(SDL_GPUCopyPass *pass, SDL_GPUTexture *texture, const cv::Mat &mat, uint32_t layer = 0)
        {
            cv::Mat rgba;
            cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGBA);

            void *ptr = SDL_MapGPUTransferBuffer(gpu_device, transfer_buffer, true);
            std::memcpy(ptr, rgba.data, rgba.total() * rgba.elemSize());
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
