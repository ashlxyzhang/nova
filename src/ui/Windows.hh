#pragma once
#ifndef WINDOWS_HH
#define WINDOWS_HH

#include "data/DataSource.hh"
#include "render/DigitalCodedExposure.hh"
#include "render/Visualizer.hh"
#include "render/GPUDevice.hh"
#include <memory>
#include <SDL3/SDL.h>

class Window {
	public:
		Window(GPUDevice& gpu_device, int width, int height, std::string title="");
		~Window();
		void close();
		bool is_open();
		void wait_for_close();
		void render(DataSource& data_source);

	private:
		GPUDevice& gpu_device;
		SDL_Window* window;
		std::string title;
		bool open_flag = false;

		ImGuiContext* imgui_context = nullptr;

		void init_imgui();
		virtual void draw(DataSource& data_source) = 0;
};


class DCEDisplay : public Window {
	public:
		DCEDisplay(GPUDevice& gpu_device, int width, int height, std::string title="");

	private:
		void draw(DataSource& data_source) override;

};


#endif