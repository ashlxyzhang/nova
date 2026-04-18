#pragma once
#ifndef GPU_DEVICE_HH
#define GPU_DEVICE_HH

#include "util/pch.hh"

/**
 * Internally, all of NOVA utilizes a SDL_GPUDevice* for rendering and compute. 
 * For easy of use in the API, however, GPUDevice acts as a wrapper for all of the
 * SDL nonsense by owning all windows it creates and ensuring everything is freed
 * in the correct order.
 * 
 * GPUDevice is a non-copyable, moveable, RAII class
 */
class GPUDevice
{
    private:
        SDL_GPUDevice *gpu_device = nullptr;
        std::vector<SDL_Window*> windows;

    public:

        GPUDevice() {
            if (!SDL_Init(SDL_INIT_VIDEO))
            {
                SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
                return;
            }

            gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");
            if (gpu_device == nullptr)
            {
                SDL_Log("Couldn't create GPU device: %s", SDL_GetError());
                return;
            }
        }

        ~GPUDevice() {
            if (gpu_device != nullptr)
            {
                SDL_WaitForGPUIdle(gpu_device);
                for (SDL_Window* window : windows) {
                    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
                    SDL_DestroyWindow(window);
                }
                SDL_DestroyGPUDevice(gpu_device);
            }
        }

        GPUDevice(const GPUDevice&) = delete;
        
        GPUDevice& operator=(const GPUDevice&) = delete;

        GPUDevice(GPUDevice&& other) noexcept {
            gpu_device = other.gpu_device;
            windows = other.windows;
            other.gpu_device = nullptr;
        }

        GPUDevice& operator=(GPUDevice&& other) noexcept {
            if (this != &other) {
                if (gpu_device) {
                    SDL_WaitForGPUIdle(gpu_device);
                    for (SDL_Window* window : windows) {
                        SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
                        SDL_DestroyWindow(window);
                    }
                    SDL_DestroyGPUDevice(gpu_device);
                }

                gpu_device = other.gpu_device;
                windows = other.windows;
                other.gpu_device = nullptr;
            }
            return *this;
        }



        bool is_open() const {
            return gpu_device != nullptr;
        }

        SDL_GPUDevice* get_SDL_device() const { 
            return gpu_device; 
        }

        SDL_Window* create_window(int width, int height, SDL_WindowFlags flags, std::string title) {

            SDL_Window* window = SDL_CreateWindow(title.c_str(), width, height, flags);
            if (window == nullptr) {
                SDL_Log("Couldn't create window: %s", SDL_GetError());
                return nullptr;
            }

            if (!SDL_ClaimWindowForGPUDevice(gpu_device, window))
            {
                SDL_Log("Couldn't claim window for GPU device: %s", SDL_GetError());
                SDL_DestroyGPUDevice(gpu_device);
                gpu_device = nullptr;
                return nullptr;
            }

            SDL_SetGPUSwapchainParameters(gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
            
            windows.push_back(window);
            return window;
        }

        void free_window(SDL_Window* window) {
            auto it = std::find(windows.begin(), windows.end(), window);
            if (it != windows.end()) {
                SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
                windows.erase(it);
            }
            SDL_DestroyWindow(window);
        }
};

#endif // GPU_DEVICE_HH