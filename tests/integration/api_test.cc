
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
	state.loop = false;

	// Display output
	DCEDisplay display(gpu, 1280, 720, "DCE Test");
	display.show(aedat_source);
}
