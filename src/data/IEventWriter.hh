#pragma once
#ifndef I_EVENT_WRITER_HH
#define I_EVENT_WRITER_HH

#include <vector>
#include <glm/glm.hpp>

namespace nova {

class IEventWriter {
	protected:
		std::string path;
		cv::Size resolution;

	public:
		IEventWriter(const std::string& path, int width, int height): path(path), resolution(width, height) {}
		virtual ~IEventWriter() = default;
		virtual void write_event_batch(std::vector<glm::vec4>& batch) = 0;
};

#endif

} // namespace nova
