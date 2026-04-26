#pragma once
#ifndef DV_EVENT_WRITER_HH
#define DV_EVENT_WRITER_HH

#include "data/IEventWriter.hh"
#include <dv-processing/io/mono_camera_writer.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <string>
#include <vector>
#include <filesystem>
#include <memory>

class DVEventWriter : public IEventWriter {
	private:
		std::unique_ptr<dv::io::MonoCameraWriter> writer;	

	public:
		DVEventWriter(const std::string& path, int width, int height) {
			this->path = path;
			this->width = width;
			this->height = height;
			const cv::Size resolution(width, height);

			// Create config object
        	dv::io::MonoCameraWriter::Config config("NOVA");

			 // Add an event stream with a resolution
    		config.addEventStream(resolution);

			std::filesystem::path p(path);
			writer = std::make_unique<dv::io::MonoCameraWriter>(p, config);
		}

		void write_event_batch(std::vector<glm::vec4>& batch) override {	
			
			// Convert to dv::EventStore
			dv::EventStore events;
			int64_t total_time = 0;

			for (const auto &vec : batch) {
				total_time += static_cast<int64_t>(vec.z);
				
				events.emplace_back(
					static_cast<int32_t>(vec.x),
					static_cast<int32_t>(vec.y),
					total_time, 
					static_cast<uint_8>(vec.w)
				);
			}
						
			try {
				// Write to file
				writer->writeEvents(events);
			} catch (const std::runtime_error& e) { 
        		std::cout << "Error: " << e.what() << std::endl;
    		} catch (...) {
				std::cout << "OH NO" << std::endl;
			}

			std::cout << "Successful write" << std::endl;
		}

};

#endif