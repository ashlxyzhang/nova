
#include "ui/Windows.hh"
#include <algorithm>

Window::Window(GPUDevice& gpu_device, int width, int height, std::string title) 
	: gpu_device(gpu_device), title(title), open_flag(false), running(false), width(width), height(height) {}

Window::~Window() {
	close();
}

void Window::init_imgui() {

	// Setup this window's own ImGui context
	IMGUI_CHECKVERSION();
	imgui_context = ImGui::CreateContext();
	ImGui::SetCurrentContext(imgui_context);

	// Configure context IO
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	// Configure context font
	void *font_memory = malloc(sizeof CascadiaCode_ttf);
	std::memcpy(font_memory, CascadiaCode_ttf, sizeof CascadiaCode_ttf);
	io.Fonts->AddFontFromMemoryTTF(font_memory, sizeof CascadiaCode_ttf, 16.0f);

	// Configure context style
	float scaling_factor = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
	ImGuiStyle &style = ImGui::GetStyle();
	style.ScaleAllSizes(scaling_factor);
	style.FontScaleDpi = scaling_factor;
	io.ConfigDpiScaleFonts = true;
	io.ConfigDpiScaleViewports = true;

	// Setup Platform/Renderer backends
	ImGui_ImplSDL3_InitForSDLGPU(window);
	ImGui_ImplSDLGPU3_InitInfo init_info = {.Device = gpu_device.get_SDL_device(),
											.ColorTargetFormat =
												SDL_GetGPUSwapchainTextureFormat(gpu_device.get_SDL_device(), window),
											.MSAASamples = SDL_GPU_SAMPLECOUNT_1,
											.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
											.PresentMode = SDL_GPU_PRESENTMODE_VSYNC};
	ImGui_ImplSDLGPU3_Init(&init_info);
}

bool Window::open() {
	if (open_flag) {
		return true;
	}

	// Initialize window
	window = gpu_device.create_window(width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	if (window == nullptr) {
		SDL_Log("Couldn't create window: %s", SDL_GetError());
		open_flag = false;
		return false;
	}

	// Initialize imgui context
	init_imgui();

	open_flag = true;
	return true;
}

void Window::close() {
	if (!open_flag) {
		return;
	}

	if (imgui_context) {
		ImGui::SetCurrentContext(imgui_context);
		ImGui_ImplSDLGPU3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext(imgui_context);
		imgui_context = nullptr;
	}

	if (window) {
		gpu_device.free_window(window);
		window = nullptr;
	}

	open_flag = false;
}

bool Window::is_open() {
	return open_flag;
}

void Window::render(DataSource& data_source) {
	if (!is_open() && !open()) {
		std::cout << "Failed to open window" << std::endl;
		return;
	}

	// Set current ImGui context to this window's context
	ImGui::SetCurrentContext(imgui_context);

	// Acquire swapchain texture
	SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device.get_SDL_device());
	SDL_GPUTexture *swapchain_texture;
	SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, nullptr, nullptr);
	
	// If valid, continue with rendering
	if (swapchain_texture != nullptr)
	{	
		// Prepare the draw data
		ImGui_ImplSDLGPU3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		draw(data_source); // Overriden by child class
		ImGui::Render();
		ImDrawData *draw_data = ImGui::GetDrawData();
		ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

		// Render pass
		SDL_GPUColorTargetInfo target_info = {};
		target_info.texture = swapchain_texture;
		target_info.clear_color = SDL_FColor{0.45f, 0.55f, 0.60f, 1.00f};
		target_info.load_op = SDL_GPU_LOADOP_CLEAR;
		target_info.store_op = SDL_GPU_STOREOP_STORE;
		target_info.mip_level = 0;
		target_info.layer_or_depth_plane = 0;
		target_info.cycle = true;
		SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);
		ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);
		SDL_EndGPURenderPass(render_pass);
	}

	SDL_SubmitGPUCommandBuffer(command_buffer);
}

void Window::show(DataSource& data_source, int fps) {
	if (!is_open() && !open()) {
		std::cout << "Failed to open window" << std::endl;
		return;
	}

	double delay = 1000 / (double) fps;
	running = true;

	while (running) {
		render(data_source);
		poll_events();        
		data_source.update_scrubber();
		
		SDL_Delay(delay);
	}
}

void Window::poll_events() {
	ImGui::SetCurrentContext(imgui_context);

	// Done this way to make it easier for API users the freedom to poll for events of a specific window
	// without destroying events for other windows. Could potentially cause event queue to reach limit 
	// but it doesn't seem likely. Blame SDL for not having a per-window event queues ffs
	std::vector<SDL_Event> extra_events;
	
	// For each event in queue
	SDL_Event event;
	while (SDL_PollEvent(&event)) {

		// If the event belongs to this window, read it with imgui, and then call handle_event
		if (SDL_GetWindowFromEvent(&event) == window) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			handle_event(event);
		} 
		// Otherwise, record to add back to queue
		else {
			extra_events.push_back(event);
		}
	}

	// Add back unrelated events to queue
	for (SDL_Event event: extra_events) {
		SDL_PushEvent(&event);
	}
}

void Window::handle_event(const SDL_Event& event) {
	if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
		running = false;
		close();
	}
}

bool Window::check_windows(std::vector<Window*>& windows) {
	// Ensure all windows valid and open to begin with
	for (Window* window: windows) {
		if (window == nullptr) {
			std::cout << "play_all() was passed a nullptr" << std::endl;
			return false;
		}

		if (std::count(windows.begin(), windows.end(), window) > 1) {
			std::cout << "Duplicate windows passed to show_all(): all windows must be unique" << std::endl;
			return false;
		}

		if (!window->is_open()) {
			window->open();
		}
	}

	return true;
}

void Window::show_all(std::vector<Window*> windows, DataSource& data_source, int fps) {

	// Ensure windows are all valid
	if (!check_windows(windows)) {
		return;
	}

	// Ensure data source is open
	if (!data_source.is_open()) {
		std::cout << "DataSource passed to Window::show() was not properly opened" << std::endl;
		return;
	}

	double delay = 1000 / (double) fps;	
	bool any_open = true;

	// Continue to render all open windows until all are closed
	while (any_open) {
		any_open = false;
		
		for (Window* window: windows) {
			if (window->is_open()) {
				any_open = true;
				window->render(data_source);
				window->poll_events();
			}
		}

		data_source.update_scrubber();
		SDL_Delay(delay);
	}
}

void Window::show_all(std::vector<Window*> windows, std::vector<DataSource*> data_sources, int fps) {

	// Ensure the lists are the same size
	if (windows.size() != data_sources.size()) {
		std::cout << "show_all(): size of window list != size of data source list" << std::endl;
		return;
	}

	// Ensure windows are valid
	if (!check_windows(windows)) {
		return;
	}

	// show_all() supports the same data source in multiple windows but it needs to make sure that
	// update_scrubber() is only called once per source
	std::vector<DataSource*> unique_sources;
	for (DataSource* data_source: data_sources) {
		if (!data_source->is_open()) {
			std::cout << "DataSource passed to Window::show() was not properly opened" << std::endl;
			return;
		}

		// Add to the list of unique sources if it hasn't been before
		if (std::find(unique_sources.begin(), unique_sources.end(), data_source) != unique_sources.end()) {
			unique_sources.push_back(data_source);
		}
	}



	double delay = 1000 / (double) fps;	
	bool any_open = true;
	int num_windows = (int) windows.size();

	// Continue to render all open windows until all are closed
	while (any_open) {
		any_open = false;

		// Render each data source to its respective window
		for (int window_index=0; window_index<num_windows; window_index++) {
			Window* window = windows[window_index];
			DataSource* data_source = data_sources[window_index];

			if (window->is_open()) {
				any_open = true;
				window->render(*data_source);
				window->poll_events();
			}
		}

		// Update all unique data sources
		for (DataSource* data_source: unique_sources) {
			data_source->update_scrubber();
		}

		SDL_Delay(delay);
	}
}

int Window::get_width() {
	return width;
}

int Window::get_height() {
	return height;
}

ImVec2 Window::fit_texture_to_space(RenderTarget& render_target, ImVec2 available_space) {
	ImVec2 display_size = available_space;      
	float aspect_ratio = render_target.width / (float) render_target.height;
	float pane_aspect = available_space.x / available_space.y;
	
	if (aspect_ratio > pane_aspect)
	{
		display_size.y = available_space.x / aspect_ratio;
	}
	else
	{
		display_size.x = available_space.y * aspect_ratio;
	}
	float x_pad = (available_space.x - display_size.x) * 0.5f;
	float y_pad = (available_space.y - display_size.y) * 0.5f;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x_pad);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_pad);
	return display_size;
}



DCEDisplay::DCEDisplay(GPUDevice& gpu_device, int width, int height, std::string title)
	: Window(gpu_device, width, height, title), dce(std::make_unique<DigitalCodedExposure>(gpu_device)) {}

void DCEDisplay::draw(DataSource& data_source) {

	// Render the new DCE output
	dce->render(data_source);
	RenderTarget& output = data_source.dce_render_targets.output;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize
								| ImGuiWindowFlags_NoMove
								| ImGuiWindowFlags_NoScrollbar
								| ImGuiWindowFlags_NoCollapse
								| ImGuiWindowFlags_NoNav
								| ImGuiWindowFlags_NoBringToFrontOnFocus
								| ImGuiWindowFlags_NoDocking;

	ImGui::Begin(title.c_str(), nullptr, window_flags);
	if (output.texture)
	{
		ImVec2 pane_size = ImGui::GetContentRegionAvail();
		ImGui::Image((ImTextureID) output.texture, Window::fit_texture_to_space(output, pane_size));
	}
	else
	{
		ImGui::Text("No Event Data.");
	}

	ImGui::End();
}

void DCEDisplay::handle_event(const SDL_Event& event) {
	Window::handle_event(event);
}


VisualizerDisplay::VisualizerDisplay(GPUDevice& gpu_device, int width, int height, std::string title)
	: Window(gpu_device, width, height, title), visualizer(std::make_unique<Visualizer>(gpu_device)) {}

void VisualizerDisplay::draw(DataSource& data_source) {

	// Render the new visualizer texture
	visualizer->render(data_source);
	RenderTarget& color_target = data_source.visualizer_render_targets.color;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize
								| ImGuiWindowFlags_NoMove
								| ImGuiWindowFlags_NoScrollbar
								| ImGuiWindowFlags_NoCollapse
								| ImGuiWindowFlags_NoNav
								| ImGuiWindowFlags_NoBringToFrontOnFocus
								| ImGuiWindowFlags_NoDocking;

	ImGui::Begin(title.c_str(), nullptr, window_flags);
	if (color_target.texture)
	{
		ImVec2 pane_size = ImGui::GetContentRegionAvail();
		ImGui::Image((ImTextureID) color_target.texture, Window::fit_texture_to_space(color_target, pane_size));
	}
	else
	{
		ImGui::Text("No texture available");
	}

	ImGui::End();
}

void VisualizerDisplay::handle_event(const SDL_Event& event) {
	Window::handle_event(event);

	// Clicks mouse
	if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) 
	{
		is_mouse_dragging = true;
	}

	// Drags mouse
	else if (event.type == SDL_EVENT_MOUSE_MOTION && is_mouse_dragging) 
	{
		float x_offset = -event.motion.xrel;
		float y_offset = event.motion.yrel;
		visualizer->rotate_camera(x_offset, y_offset);	
	}

	// Scrolls
	else if (event.type == SDL_EVENT_MOUSE_WHEEL) 
	{	
		float scroll_delta = event.wheel.y;
		if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
		{
			scroll_delta = -scroll_delta;
		}

		if (scroll_delta != 0.0f)
		{
			visualizer->zoom_camera(scroll_delta * 0.1f);
		}
	}

	// Releases click
	else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) 
	{
		is_mouse_dragging = false;
	}

	// Window loses focus or mouse leaves area
	else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE ) 
	{
		is_mouse_dragging = false;
	}
}