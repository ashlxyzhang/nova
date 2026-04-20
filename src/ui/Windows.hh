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
		bool is_open();

		void render(DataSource& data_source);			// Renders a single frame 
		void play(DataSource& data_source, int fps=60);	// Blocking rendering loop 
		
		bool open();	// Returns whether or not opening was successful
		void close();							
		
		int get_width();
		int get_height();
		
		static void play_all(std::vector<Window*> windows, DataSource& data_source, int fps=60);
		
	protected:
		GPUDevice& gpu_device;
		SDL_Window* window;
		ImGuiContext* imgui_context = nullptr;
		
		std::string title;
		bool open_flag = false;
		bool running = false;
		
		int width;
		int height;
		
		void poll_events(DataSource& data_source);
		void init_imgui();
		virtual void handle_event(const SDL_Event& event, DataSource& data_source) = 0;
		virtual void draw(DataSource& data_source) = 0;
};




class DCEDisplay : public Window {
	public:
		DCEDisplay(GPUDevice& gpu_device, int width, int height, std::string title="");
		
	private:
		std::unique_ptr<DigitalCodedExposure> dce;
		
		void handle_event(const SDL_Event& event, DataSource& data_source) override;
		void draw(DataSource& data_source) override;

};

class VisualizerDisplay : public Window {
	public:
		VisualizerDisplay(GPUDevice& gpu_device, int width, int height, std::string title="");
		
	private:
		bool is_mouse_dragging = false;
		std::unique_ptr<Visualizer> visualizer;
		
		void handle_event(const SDL_Event& event, DataSource& data_source) override;
		void draw(DataSource& data_source) override;

};


#endif