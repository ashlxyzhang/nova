#pragma once
#ifndef DATA_SOURCE_HH
#define DATA_SOURCE_HH

#include "data/EventData.hh"
#include "render/RenderTarget.hh"
#include "ui/Scrubber.hh"
#include "data/IEventReader.hh"

#include <string>
#include <dv-processing/io/camera/discovery.hpp>
#include <dv-processing/io/camera/usb_device.hpp>

struct DataSource {
	mutable std::shared_mutex mutex;
	SDL_GPUDevice* gpu_device;
    std::string name; 

    enum Type { CAMERA, FILE } type;
    enum State { PAUSED, ACTIVE, FAILED_TO_OPEN } state = State::PAUSED; 

	std::unique_ptr<IEventReader> reader;
    EventData event_data;
    Scrubber scrubber;
    
    struct {
		// DCE
		RenderTarget positive_values_texture;
		RenderTarget negative_values_texture;
        RenderTarget dce;

		// Visualizer
        RenderTarget visualizer_color;
		RenderTarget visualizer_depth;
    } render_targets;
    
    float event_discard_odds = 1.0f;
	cv::Size resolution;

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
