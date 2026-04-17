#pragma once
#ifndef WINDOWS_HH
#define WINDOWS_HH

#include "data/DataSource.hh"
#include "render/DigitalCodedExposure.hh"
#include "render/Visualizer.hh"
#include <memory>
#include <SDL3/SDL.h>

class Window {
	public:
		Window(SDL_GPUDevice* gpu_device, int width, int height, std::string title="");
		~Window();

		virtual void render(std::shared_ptr<DataSource> data_source) = 0;
		virtual void render(const DataSource& data_source) = 0;
		
		void close();

	private:
		SDL_Window* window;
		SDL_GPUDevice* gpu_device;
};


class DCEDisplay : public Window {
	public:
		void render(std::shared_ptr<DataSource> data_source) override;
		void render(const DataSource& data_source) override;
	
	private:
};


// class VisualizerDisplay : public Window {
// 	public:
// 		void render(std::shared_ptr<DataSource> data_source) override;
// 		void render(const DataSource& data_source) override;

// 	private:
// };



#endif