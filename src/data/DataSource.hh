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
	friend class DataAcqusition;
	friend class GUI;

	public:
		enum class Vendor {
			DV, 
			PROPHESEE
		};
		
		enum class Type {
			CAMERA, 
			FILE
		};
		
		struct ScannedCamera {
			Vendor vendor;
			dv::io::camera::USBDevice::DeviceDescriptor dv_descriptor{};
			std::string prophesee_serial; 
        };


		// Data Management
		Scrubber scrubber;						// Data selection (made public b/c I trust you 😁)

		// Rendering parameters
		DigitalCodedExposure::Parameters dce_parameters;
		Visualizer::Parameters visualizer_parameters;
	
		// Rendering
		DigitalCodedExposure::RenderTargets dce_render_targets;
		Visualizer::RenderTargets visualizer_render_targets;

		// Constructors
		DataSource(GPUDevice& gpu_device, const std::string& file_path);
		DataSource(GPUDevice& gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera);
		DataSource(GPUDevice& gpu_device, const MetavisionEventReader::LiveCamera& camera);
		DataSource(GPUDevice& gpu_device, const ScannedCamera& scanned_camera);
		DataSource(SDL_GPUDevice* gpu_device, const std::string& file_path);
		DataSource(SDL_GPUDevice* gpu_device, const dv::io::camera::USBDevice::DeviceDescriptor& camera);
		DataSource(SDL_GPUDevice* gpu_device, const MetavisionEventReader::LiveCamera& camera);
		~DataSource();

		// Updates scrubber
		void update();
		
		// Single batches
		size_t get_batch_event_data(float event_discard_odds=0.0f);
		size_t get_batch_frame_data();

		// Reading 
		void read(float event_discard_odds=0.0f, bool blocking=true);		
		void stop_reading_thread();		
		void wait_reading_thread();		

		// Writing 
		void save_to_file(const std::string& path, bool blocking=true);
		void save_to_file_by_time(const std::string& path, float start_time, float end_time, bool blocking=true);
		void save_to_file_by_index(const std::string& path, size_t start_index, size_t end_index, bool blocking=true);
		void stop_writing_thread();
		void wait_writing_thread();				

		// cv::Mat outputs
		cv::Mat save_dce_output();
		cv::Mat save_visualizer_output();
		
		// Status
		bool is_open();
		bool is_eof();
		bool is_reading();
		bool is_writing();

		// Other
		std::string get_name();
		Type get_type();
		cv::Size get_resolution();
		size_t size();
		Vendor get_vendor();
		static std::vector<ScannedCamera> get_attached_cameras();
		EventData* get_ptr_to_event_data();


	private:
		SDL_GPUDevice* gpu_device;

		// Metadata
		std::string name; 
		cv::Size resolution;
		Type type;
		Vendor vendor;

		// Helper threads
		std::thread reading_thread;
		std::thread writing_thread;
		
		// State
		std::atomic<bool> reading = false;
		std::atomic<bool> writing = false;
		std::atomic<bool> read_to_end = false;
		bool is_open_ = false;
		
		// Data management
		std::unique_ptr<IEventReader> reader; 	// Data input
		TransferBuffer transfer_buffer;			// GPU reading and writing
		EventData event_data; 					// Event storage

		// Internal helpers
		cv::Mat texture_to_cvmat(SDL_GPUTexture* texture, SDL_GPUTextureFormat texture_format, int width, int height);
		void init_render_targets();
		void reading_loop(float event_discard_odds=0.0f);
		void init();
};


#endif // DATA_SOURCE_HH
