#pragma once
#ifndef DATA_SOURCE_HH
#define DATA_SOURCE_HH

#include "data/EventData.hh"
#include "render/RenderTarget.hh"
#include "ui/Scrubber.hh"
#include "data/IEventReader.hh"
#include "render/DigitalCodedExposure.hh"
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

	// Data acquisition and storage
    float event_discard_odds = 1.0f;
	cv::Size resolution;
	std::unique_ptr<IEventReader> reader;
    EventData event_data;
	Scrubber scrubber;

	// Rendering parameters and outputs
	SDL_GPUDevice* gpu_device;
	DigitalCodedExposure::Parameters dce_parameters;
	DigitalCodedExposure::RenderTargets dce_render_targets;
	Visualizer::Parameters visualizer_parameters;
	Visualizer::RenderTargets visualizer_render_targets;
    

	DataSource(SDL_GPUDevice* gpu_device, const std::string& file_path);
	DataSource(SDL_GPUDevice* gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera);
	~DataSource();

	void init_render_targets();
	void get_batch_event_data();
	void get_batch_frame_data();
	void update();
	bool is_open();
};


#endif // DATA_SOURCE_HH
