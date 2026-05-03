#pragma once
#ifndef TRANSFER_BUFFER_HH
#define TRANSFER_BUFFER_HH

#include "util/pch.hh"

namespace nova {


/**
 * @brief Class for transfering data between CPU and GPU.
 */
class TransferBuffer
{
    private:
        SDL_GPUDevice* gpu_device = nullptr;

        SDL_GPUTransferBuffer* upload_buffer = nullptr;
        Uint32 upload_capacity = 0;
        
        SDL_GPUTransferBuffer* download_buffer = nullptr;
        Uint32 download_capacity;

        constexpr static Uint32 initial_upload_capacity = 1 << 20;    
        constexpr static Uint32 initial_download_capacity = 0; // Not all users need downloading, lazily allocate

        static Uint32 next_pow2(Uint32 v)
        {
            Uint32 r = 1;
            while (r < v)
                r <<= 1;
            return r;
        }

        /**
         * @brief Grows the upload buffer if the current capacity is too small.
         * @param nbyte Required capacity in bytes.
         */
        void ensure_upload_capacity(size_t nbyte)
        {
            if (nbyte <= upload_capacity) {
                return;
            }

            if (upload_buffer) {   
                SDL_ReleaseGPUTransferBuffer(gpu_device, upload_buffer);
            }

            upload_capacity = next_pow2(static_cast<Uint32>(nbyte));
            SDL_GPUTransferBufferCreateInfo info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = upload_capacity};
            upload_buffer = SDL_CreateGPUTransferBuffer(gpu_device, &info);
        }


        /**
         * @brief Grows the download buffer if the current capacity is too small.
         * @param nbyte Required capacity in bytes.
         */
        void ensure_download_capacity(size_t nbyte) 
        {
            if (nbyte <= download_capacity) {
                return;
            }

            if (download_buffer) {
                SDL_ReleaseGPUTransferBuffer(gpu_device, download_buffer);
            }
            
            download_capacity = next_pow2(static_cast<Uint32>(nbyte));
            SDL_GPUTransferBufferCreateInfo info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = download_capacity};
            download_buffer = SDL_CreateGPUTransferBuffer(gpu_device, &info);
        }

    public:
        /**
         * @brief Constructor. Initializes GPUTransferBuffer for transferring data to GPU.
         * @param gpu_device SDL_GPUDevice to upload data to.
         */
        TransferBuffer(SDL_GPUDevice *gpu_device): gpu_device(gpu_device)
        {
            ensure_upload_capacity(initial_upload_capacity);
            ensure_download_capacity(initial_download_capacity);
        }

        /**
         * @brief Destructor. Releases data buffer on GPU.
         */
        ~TransferBuffer()
        {
            if (upload_buffer) {
                SDL_ReleaseGPUTransferBuffer(gpu_device, upload_buffer);
            }

            if (download_buffer) {
                SDL_ReleaseGPUTransferBuffer(gpu_device, download_buffer);
            }
        }

        /**
         * @brief Uploads buffer data to GPU.
         * @param pass SDL_GPUCopyPass for copying data to GPU.
         * @param dst Destination of data to be copied to GPU.
         * @param src Source data buffer to be copied.
         * @param nbyte Number of bytes to copy from src to GPU.
         */
        void upload_to_gpu(SDL_GPUCopyPass *pass, SDL_GPUBuffer *dst, const void *src, size_t nbyte)
        {
            if (nbyte == 0) {
                return;
            }

            ensure_upload_capacity(nbyte);

            void *ptr = SDL_MapGPUTransferBuffer(gpu_device, upload_buffer, true);
            SDL_memcpy(ptr, src, nbyte);
            SDL_UnmapGPUTransferBuffer(gpu_device, upload_buffer);

            SDL_GPUTransferBufferLocation transfer_location = {.transfer_buffer = upload_buffer};
            SDL_GPUBufferRegion buffer_region = {.buffer = dst, .offset = 0, .size = static_cast<Uint32>(nbyte)};
            SDL_UploadToGPUBuffer(pass, &transfer_location, &buffer_region, false);
        }

        //
        /**
         * @brief Uploads texture to GPU.
         *
         * This function will make a lot of assumptions
         * first is that the SDL_GPUTexture is the same resolution as the cv::Mat
         * second is that they have the same number of bits per channel
         * third is that the texture was created with rgba8 unorm format
         * @param pass SDL_GPUCopyPass for copying data to GPU.
         * @param texture SDL_GPUTexture texture destination to upload texture to.
         * @param mat Texture information as cv::Mat to upload.
         * @param layer Texture layers.
         */
        void upload_cv_mat(SDL_GPUCopyPass *pass, SDL_GPUTexture *texture, const cv::Mat &mat, uint32_t layer = 0)
        {
            cv::Mat rgba;
            cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGBA);

            size_t nbyte = rgba.total() * rgba.elemSize();
            ensure_upload_capacity(nbyte);

            void *ptr = SDL_MapGPUTransferBuffer(gpu_device, upload_buffer, true);
            std::memcpy(ptr, rgba.data, nbyte);
            SDL_UnmapGPUTransferBuffer(gpu_device, upload_buffer);

            SDL_GPUTextureTransferInfo source_texture_transfer_info = {};
            source_texture_transfer_info.transfer_buffer = upload_buffer;
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

        /**
         * @brief Downloads GPU texture to cv::Mat.
         * @param pass SDL_GPUCopyPass for copying data from GPU.
         * @param texture SDL_GPUTexture source to download from.
         * @param width Texture width.
         * @param height Texture height.
         * @param layer Texture layer.
         * @return cv::Mat containing the downloaded texture in BGR format.
         */
        cv::Mat download_to_cv_mat(SDL_GPUCopyPass *pass, SDL_GPUTexture *texture, SDL_GPUTextureFormat format,
                                    uint32_t width, uint32_t height, uint32_t layer = 0)
        {
            // Calculate required buffer size (RGBA8: 4 bytes per pixel)
            const Uint32 bytes_per_pixel = 4;
            const Uint32 required_bytes = static_cast<uint64_t>(width) * height * bytes_per_pixel;
            
            ensure_download_capacity(required_bytes);

            // Download to cpu buffer
            SDL_GPUTextureRegion source_region = {
                .texture = texture,
                .mip_level = 0,
                .layer = layer,
                .x = 0, .y = 0, .z = 0,
                .w = width,
                .h = height,
                .d = 1
            };

            SDL_GPUTextureTransferInfo dest_info = {
                .transfer_buffer = download_buffer,
                .offset = 0,
                .pixels_per_row = width,
                .rows_per_layer = height
            };

            SDL_DownloadFromGPUTexture(pass, &source_region, &dest_info);
            void *ptr = SDL_MapGPUTransferBuffer(gpu_device, download_buffer, false);

            
            // Format the cpu buffer into appropriate cv::mat type
            cv::Mat result;
            switch (format) {
                case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM: {
                    cv::Mat rgba(height, width, CV_8UC4, ptr);
                    cv::cvtColor(rgba, result, cv::COLOR_RGBA2BGR);
                    break;
                }
                

                // Visualizer outputs a SNORM texture for some reason, converting here to avoid
                // accidently breaking the Visualizer :P
                case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM: {
                    cv::Mat rgba_signed(height, width, CV_8SC4, ptr);
                    cv::Mat rgba_unsigned;
                    rgba_signed.convertTo(rgba_unsigned, CV_8UC4, 1.0, 128);
                    cv::cvtColor(rgba_unsigned, result, cv::COLOR_RGBA2BGR);
                    break;
                }
            
            default:
                throw std::runtime_error("Unsupported texture format");
            }

            result = result.clone(); // Make deep copy before unmapping
            SDL_UnmapGPUTransferBuffer(gpu_device, download_buffer);
            return result;
        }
};

#endif

} // namespace nova
