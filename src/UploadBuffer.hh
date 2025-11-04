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
};

#endif