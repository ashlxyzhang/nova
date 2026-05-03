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

namespace nova {


class DVEventWriter : public IEventWriter {
	private:
		std::unique_ptr<dv::io::MonoCameraWriter> writer;	

	public:
		DVEventWriter(const std::string& path, int width, int height): IEventWriter(path, width, height) {
			const cv::Size resolution(width, height);

        	const auto config = dv::io::MonoCameraWriter::EventOnlyConfig("NOVA", resolution);
			std::filesystem::path p(path);

			writer = std::make_unique<dv::io::MonoCameraWriter>(path, config);
		}

		void write_event_batch(std::vector<glm::vec4>& batch) override {	
			
			// Convert to dv::EventStore
			dv::EventStore events;
			for (const auto &vec : batch) {
				events.emplace_back(
					static_cast<int64_t>(vec.z), 
					static_cast<int32_t>(vec.x),
					static_cast<int32_t>(vec.y),
					static_cast<uint8_t>(vec.w)
				);

			}
			
			writer->writeEvents(events);
		}

};

#endif
} // namespace nova
