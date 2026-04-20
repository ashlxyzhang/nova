
#include "ui/Windows.hh"

Window::Window(GPUDevice& gpu_device, int width, int height, std::string title) 
	: gpu_device(gpu_device), title(title) {

	// Initialize SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
		open_flag = false;
		return;
	}

	// Initialize window
	window = gpu_device.create_window(width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	if (window == nullptr) {
		SDL_Log("Couldn't create window: %s", SDL_GetError());
		open_flag = false;
		return;
	}

	init_imgui();

	open_flag = true;
}

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

void Window::close() {
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
		SDL_Quit();
	}

	open_flag = false;
}

bool Window::is_open() {
	return open_flag;
}

void Window::render(DataSource& data_source) {
	if (!is_open()) {
		std::cout << "render() called on closed window: \'" << title << "\'" << std::endl;
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

void Window::play(DataSource& data_source) {
	if (!is_open()) {
		std::cout << "play() called on closed window: \'" << title << "\'" << std::endl;
		return;
	}
	
	bool running = true;
	while (running) {
		// Process SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
				running = false;
            }

			handle_event(event, data_source);
        }

		data_source.update_scrubber();
		render(data_source);
		SDL_Delay(16); // 16ms delay ~ 60fps
	}
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

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar
								| ImGuiWindowFlags_NoResize
								| ImGuiWindowFlags_NoMove
								| ImGuiWindowFlags_NoScrollbar
								| ImGuiWindowFlags_NoCollapse
								| ImGuiWindowFlags_NoNav
								| ImGuiWindowFlags_NoBringToFrontOnFocus
								| ImGuiWindowFlags_NoDocking
								| ImGuiWindowFlags_NoBackground;

	ImGui::Begin("DCE", nullptr, window_flags);
	if (output.texture)
	{
		ImVec2 pane_size = ImGui::GetContentRegionAvail();
		
		// Draw image
		ImGui::Image((ImTextureID) output.texture, pane_size);
	}
	else
	{
		ImGui::Text("No Event Data.");
	}

	ImGui::End();
}

void DCEDisplay::handle_event(const SDL_Event& event, DataSource& data_source) {}


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

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar
								| ImGuiWindowFlags_NoResize
								| ImGuiWindowFlags_NoMove
								| ImGuiWindowFlags_NoScrollbar
								| ImGuiWindowFlags_NoCollapse
								| ImGuiWindowFlags_NoNav
								| ImGuiWindowFlags_NoBringToFrontOnFocus
								| ImGuiWindowFlags_NoDocking
								| ImGuiWindowFlags_NoBackground;

	ImGui::Begin("3D Visualizer", nullptr, window_flags);
	if (color_target.texture)
	{
		
		ImVec2 pane_size = ImGui::GetContentRegionAvail();
		float tex_aspect = (float)color_target.width / (float) color_target.height;
		ImVec2 display_size = pane_size;
		float pane_aspect = pane_size.x / pane_size.y;
		if (tex_aspect > pane_aspect)
		{
			display_size.y = pane_size.x / tex_aspect;
		}
		else
		{
			display_size.x = pane_size.y * tex_aspect;
		}
		float x_pad = (pane_size.x - display_size.x) * 0.5f;
		float y_pad = (pane_size.y - display_size.y) * 0.5f;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x_pad);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_pad);
		ImGui::Image((ImTextureID) color_target.texture, display_size);
		color_target.is_focused = ImGui::IsItemHovered();
	}
	else
	{
		ImGui::Text("No texture available");
	}

	ImGui::End();
}

void VisualizerDisplay::handle_event(const SDL_Event& event, DataSource& data_source) {

	// Check if selected visualizer output is focused
	bool focused = data_source.visualizer_render_targets.color.is_focused;
	
	// Start a click, scrolling, and dragging must be done while focused
	if (focused) {
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
	}

	// Releasing should be possible when not in focus
	// Releases click
	if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) 
	{
		is_mouse_dragging = false;
	}

	// Window loses focus or mouse leaves area
	else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE ) 
	{
		is_mouse_dragging = false;
	}

	// Reset focused flag
	data_source.visualizer_render_targets.color.is_focused = false;
}