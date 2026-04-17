
#include "ui/Windows.hh"

Window::Window(GPUDevice& gpu_device, int width, int height, std::string title) 
	: gpu_device(gpu_device), title(title) {

	// Initialize SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
		return;
	}

	// Initialize window
	window = gpu_device.create_window(width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, title);
	if (window == nullptr) {
		SDL_Log("Couldn't create window: %s", SDL_GetError());
		return;
	}
}

Window::~Window() {
	close();
}

void Window::close() {
	if (window) {
		gpu_device.free_window(window);
		window = nullptr;
		SDL_Quit();
	}
}