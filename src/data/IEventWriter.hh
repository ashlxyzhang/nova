#pragma once
#ifndef I_EVENT_WRITER_HH
#define I_EVENT_WRITER_HH

#include <vector>
#include <glm/glm.hpp>

class IEventWriter {
	protected:
		std::string path;
		int width;
		int height;

	public:
		virtual ~IEventWriter() = default;

		int get_width() {
			return width;
		}

		int get_height() {
			return height; 
		}

		virtual void write_event_batch(std::vector<glm::vec4>& batch) = 0;
};

#endif