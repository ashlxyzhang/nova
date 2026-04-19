
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
	window = gpu_device.create_window(width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, title);
	if (window == nullptr) {
		SDL_Log("Couldn't create window: %s", SDL_GetError());
		open_flag = false;
		return;
	}

	init_imgui();

	// Allow rendering during window operations
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

	open_flag = true;
}

Window::~Window() {
	ImGui_ImplSDLGPU3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	close();
}

void Window::init_imgui() {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	void *font_memory = malloc(sizeof CascadiaCode_ttf);
	std::memcpy(font_memory, CascadiaCode_ttf, sizeof CascadiaCode_ttf);
	io.Fonts->AddFontFromMemoryTTF(font_memory, sizeof CascadiaCode_ttf, 16.0f);

	// Setup scaling
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
	if (window) {
		gpu_device.free_window(window);
		window = nullptr;
		SDL_Quit();
	}
}

bool Window::is_open() {
	return open_flag;
}

void Window::wait_for_close() {
	while (is_open()) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				close();
			}
		}
		SDL_Delay(100);
	}
}

void Window::render(DataSource& data_source) {

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



DCEDisplay::DCEDisplay(GPUDevice& gpu_device, int width, int height, std::string title)
	: Window(gpu_device, width, height, title) {}

void DCEDisplay::draw(DataSource& data_source) {
	RenderTarget& output = data_source.dce_render_targets.output;

	ImGui::Begin("Frame");
	if (output.texture)
	{
        ImGui::Text("Digital Coded Exposure");

		ImVec2 pane_size = ImGui::GetContentRegionAvail();
		ImVec2 display_size = pane_size;
		
		float aspect_ratio = output.width / (float)output.height;
		float pane_aspect = pane_size.x / pane_size.y;
		if (aspect_ratio > pane_aspect)
		{
			display_size.y = pane_size.x / aspect_ratio;
		}
		else
		{
			display_size.x = pane_size.y * aspect_ratio;
		}
		float x_pad = (pane_size.x - display_size.x) * 0.5f;
		float y_pad = (pane_size.y - display_size.y) * 0.5f;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x_pad);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_pad);


		// Draw image
		ImGui::Image((ImTextureID) output.texture, display_size);
	}
	else
	{
		ImGui::Text("No Event Data.");
	}

	ImGui::End();
}
