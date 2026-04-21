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

#include <string>
#include <dv-processing/io/camera/discovery.hpp>
#include <dv-processing/io/camera/usb_device.hpp>

struct DataSource {

	mutable std::shared_mutex mutex;

	// State of source
    std::string name; 
    enum Type { CAMERA, FILE } type;
    enum State { PAUSED, ACTIVE, FAILED_TO_OPEN } state = State::PAUSED; 
	cv::Size resolution;

	// Data acquisition and storage
	std::unique_ptr<IEventReader> reader;
    EventData event_data;
	Scrubber scrubber;
    float event_discard_odds = 0.0f;
	
	std::atomic<bool> reading_thread_running = false;
	std::thread reading_thread;

	// Rendering parameters and outputs
	SDL_GPUDevice* gpu_device;
	DigitalCodedExposure::Parameters dce_parameters;
	DigitalCodedExposure::RenderTargets dce_render_targets;
	Visualizer::Parameters visualizer_parameters;
	Visualizer::RenderTargets visualizer_render_targets;
    
	// Used internally
	DataSource(SDL_GPUDevice* gpu_device, const std::string& file_path);
	DataSource(SDL_GPUDevice* gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera);
	DataSource(SDL_GPUDevice* gpu_device, const MetavisionEventReader::LiveCamera& camera);

	// Used externally by API (so that user doesn't have to know about SDL_GPUDevice)
	DataSource(GPUDevice& gpu_device, const std::string& file_path);
	DataSource(GPUDevice& gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera);
	DataSource(GPUDevice& gpu_device, const MetavisionEventReader::LiveCamera& camera);
	
	~DataSource();

	void init_render_targets();
	size_t get_batch_event_data();
	size_t get_batch_frame_data();
	void update_scrubber();
	bool is_open();

	void read_all(); 				// Blocking
	void start_reading_thread();	// Non-blocking
	void wait_reading_thread();		// Blocking
	void stop_reading_thread();		// Non-blocking
};


#endif // DATA_SOURCE_HH
