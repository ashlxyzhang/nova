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
		
		void show(DataSource& data_source, int fps=60);	
		static void show_all(std::vector<Window*> windows, DataSource& data_source, int fps=60);
		static void show_all(std::vector<Window*> windows, std::vector<DataSource*> data_sources, int fps=60);
		
		int get_width();
		int get_height();
		
		
	protected:
		GPUDevice& gpu_device;
		SDL_Window* window;
		ImGuiContext* imgui_context = nullptr;
		
		std::string title;
		bool open_flag = false;
		bool running = false;
		
		int width;
		int height;
		
		bool open();	
		void close();	
		bool is_open();
		
		void init_imgui();
		void poll_events();
		void render(DataSource& data_source);
		
		virtual void handle_event(const SDL_Event& event);
		virtual void draw(DataSource& data_source) = 0;

		// Helpers
		static ImVec2 fit_texture_to_space(RenderTarget& render_target, ImVec2 available_space);
		static bool check_windows(std::vector<Window*>& windows);
};




class DCEDisplay : public Window {
	public:
		DCEDisplay(GPUDevice& gpu_device, int width, int height, std::string title="");
		
	private:
		std::unique_ptr<DigitalCodedExposure> dce;
		
		void handle_event(const SDL_Event& event) override;
		void draw(DataSource& data_source) override;

};

class VisualizerDisplay : public Window {
	public:
		VisualizerDisplay(GPUDevice& gpu_device, int width, int height, std::string title="");
		
	private:
		bool is_mouse_dragging = false;
		std::unique_ptr<Visualizer> visualizer;
		
		void handle_event(const SDL_Event& event) override;
		void draw(DataSource& data_source) override;

};


#endif