
#include "data/DataSource.hh"
#include "ui/Windows.hh"
#include "ui/Scrubber.hh"
#include "render/GPUDevice.hh"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <memory>

namespace {
	std::filesystem::path fixture_path() {
		const char *root = std::getenv("NOVA_TEST_FIXTURES");
		return std::filesystem::path{root ? root : "."};
	}
} // namespace


TEST(API, FixtureExists)
{
    ASSERT_TRUE(std::filesystem::exists(fixture_path())) << "Missing fixture: " << fixture_path();
}

TEST(API, RawToAedatTest) {

	// Construct file paths
	std::string input_file = (fixture_path() / "hand_spinner.raw").string();
	std::string output_file = (fixture_path() / "hand_spinner.aedat4").string();

	// Initialize GPU device
	GPUDevice gpu;
	
	// Read .raw file
	DataSource raw_source(gpu, input_file);
	raw_source.read();

	// Write data to .aedat4 file
	raw_source.save_to_file(output_file);

	// Read back .aedat file
	DataSource aedat_source(gpu, output_file);
	aedat_source.read();

	// Configure scrubber
	Scrubber::State& state = aedat_source.scrubber.state;
	state.current_time = 10000.0f;
	state.time_window = 10000.0f;
	state.time_step = 20000.0f;    
	state.mode = Scrubber::Mode::PLAYING;  
	state.loop = true;

	// Display output
	DCEDisplay display(gpu, 1280, 720, "DCE Test");
	display.show(aedat_source);
}

TEST(API, CVMatOutput) {

	GPUDevice gpu;
	DataSource source(gpu, (fixture_path() / "hand_spinner.raw").string());
	source.read();
	std::cout << "Finished Reading!" << std::endl;

	Scrubber::State& s = source.scrubber.state;
	s.current_time = 10000.0f;
	s.time_window = 10000.0f;
	s.time_step = 20000.0f;
	s.mode = Scrubber::Mode::PLAYING;
	s.loop = false;

	DigitalCodedExposure dce(gpu);

	int i = 0;
	while (!s.is_eof() && ++i < 300) {
		source.update();
		dce.render(source);

		cv::Mat frame = source.save_dce_output();
		imshow("DCE", frame);
		cv::waitKey(0);
	}

	cv::destroyAllWindows();
}

TEST(API, TwoSourcesTwoWindows) {

	GPUDevice gpu;

	DataSource source1(gpu, (fixture_path() / "hand_spinner.raw").string());
	source1.read(0.0, false);

	DataSource source2(gpu, (fixture_path() / "pedestrians.raw").string());
	source2.read(0.0, false);

	source1.wait_reading_thread();
	source2.wait_reading_thread();
	std::cout << "Finished Reading!" << std::endl;

	Scrubber::State& s1 = source1.scrubber.state;
	s1.current_time = 10000.0f;
	s1.time_window = 10000.0f;
	s1.time_step = 20000.0f;
	s1.mode = Scrubber::Mode::PLAYING;
	s1.loop = false;

	Scrubber::State& s2 = source2.scrubber.state;
	s2.current_time = 10000.0f;
	s2.time_window = 10000.0f;
	s2.time_step = 20000.0f;
	s2.mode = Scrubber::Mode::PLAYING;
	s2.loop = false;

	DCEDisplay dce1(gpu, 1280, 720, "DCE 1");
	DCEDisplay dce2(gpu, 1280, 720, "DCE 2");
	
	Window::show_all({&dce1, &dce2}, {&source1, &source2});
}


TEST(API, OneSourceTwoWindows) {

	GPUDevice gpu;

	DataSource source(gpu, (fixture_path() / "hand_spinner.raw").string());
	source.read();
	std::cout << "Finished Reading!" << std::endl;

	Scrubber::State& s = source.scrubber.state;
	s.current_time = 10000.0f;
	s.time_window = 10000.0f;
	s.time_step = 20000.0f;
	s.mode = Scrubber::Mode::PLAYING;
	s.loop = false;

	DCEDisplay dce(gpu, 1280, 720, "DCE 1");
	VisualizerDisplay vis(gpu, 1280, 720, "DCE 2");
	
	Window::show_all({&dce, &vis}, source);
}
