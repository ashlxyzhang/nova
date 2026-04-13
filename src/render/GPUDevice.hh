#pragma once
#ifndef GPU_DEVICE_HH
#define GPU_DEVICE_HH

#include "util/pch.hh"

class GPUDevice
{
    private:
        SDL_GPUDevice *gpu_device = nullptr;
        SDL_Window *window = nullptr;
    public:
        GPUDevice() = default;

        GPUDevice(SDL_Window *window) : window(window) {
            gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");
            if (gpu_device == nullptr)
            {
                SDL_Log("Couldn't create GPU device: %s", SDL_GetError());
                return;
            }

            if (!SDL_ClaimWindowForGPUDevice(gpu_device, window))
            {
                SDL_Log("Couldn't claim window for GPU device: %s", SDL_GetError());
                SDL_DestroyGPUDevice(gpu_device);
                gpu_device = nullptr;
                return;
            }

            SDL_SetGPUSwapchainParameters(gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                          SDL_GPU_PRESENTMODE_VSYNC);
        }

        GPUDevice(const GPUDevice&) = delete;
        
        GPUDevice& operator=(const GPUDevice&) = delete;

        GPUDevice(GPUDevice&& other) noexcept {
            gpu_device = other.gpu_device;
            window = other.window;
            other.gpu_device = nullptr;
        }

        GPUDevice& operator=(GPUDevice&& other) noexcept {
            if (this != &other) {
                if (gpu_device) {
                    SDL_WaitForGPUIdle(gpu_device);
                    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
                    SDL_DestroyGPUDevice(gpu_device);
                }

                gpu_device = other.gpu_device;
                window = other.window;
                other.gpu_device = nullptr;
            }
            return *this;
        }

        ~GPUDevice() {
            if (gpu_device != nullptr)
            {
                SDL_WaitForGPUIdle(gpu_device);
                SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
                SDL_DestroyGPUDevice(gpu_device);
            }
        }

        SDL_GPUDevice* get_device() const { 
            return gpu_device; 
        }
};

#endif // GPU_DEVICE_HH