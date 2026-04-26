#pragma once
#ifndef DATA_SOURCE_HH
#define DATA_SOURCE_HH

#include "data/EventData.hh"
#include "render/RenderTarget.hh"
#include "ui/Scrubber.hh"
#include "data/IEventReader.hh"
#include "data/MetavisionEventReader.hh"
#include "render/DigitalCodedExposure.hh"
#include "render/GPUDevice.hh"
#include "render/Visualizer.hh"
#include "render/TransferBuffer.hh"

#include <string>
#include <dv-processing/io/camera/discovery.hpp>
#include <dv-processing/io/camera/usb_device.hpp>

struct DataSource {
	mutable std::shared_mutex mutex;
	SDL_GPUDevice* gpu_device;

	// Metadata
    std::string name; 
	cv::Size resolution;
    enum Type { CAMERA, FILE } type;

	// Data Management
	std::unique_ptr<IEventReader> reader; 	// Input
	std::thread reading_thread;
    EventData event_data; 					// Storage
	Scrubber scrubber;						// Selection
	TransferBuffer transfer_buffer;			// GPU transfering
    float event_discard_odds = 0.0f;		
	
	// State
    enum State { PAUSED, ACTIVE, FAILED_TO_OPEN } state = State::PAUSED; 
	std::atomic<bool> reading_thread_running = false;

	// Rendering 
	DigitalCodedExposure::Parameters dce_parameters;
	DigitalCodedExposure::RenderTargets dce_render_targets;
	Visualizer::Parameters visualizer_parameters;
	Visualizer::RenderTargets visualizer_render_targets;
    
	// Internal Constructors
	DataSource(SDL_GPUDevice* gpu_device, const std::string& file_path);
	DataSource(SDL_GPUDevice* gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera);
	DataSource(SDL_GPUDevice* gpu_device, const MetavisionEventReader::LiveCamera& camera);

	// External API Constructors
	DataSource(GPUDevice& gpu_device, const std::string& file_path);
	DataSource(GPUDevice& gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera);
	DataSource(GPUDevice& gpu_device, const MetavisionEventReader::LiveCamera& camera);
	
	~DataSource();

	bool is_open();
	void init_render_targets();
	
	size_t get_batch_event_data();
	size_t get_batch_frame_data();
	void update();

	void read_all(); 				// Blocking
	void wait_reading_thread();		// Blocking
	void start_reading_thread();	// Non-blocking
	void stop_reading_thread();		// Non-blocking

	cv::Mat save_dce_output();
	cv::Mat save_visualizer_output();
	cv::Mat texture_to_cvmat(SDL_GPUTexture* texture, SDL_GPUTextureFormat texture_format, int width, int height);
};


#endif // DATA_SOURCE_HH
