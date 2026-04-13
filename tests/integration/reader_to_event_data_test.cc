// Integration test: file (.aedat4) -> DVEventReader -> EventData.
// Exercises the highest-value data flow that doesn't require a GPU device,
// deliberately bypassing DataSource (which owns GPU upload buffers).
#include "data/DVEventReader.hh"
#include "data/EventData.hh"

#include <dv-processing/io/mono_camera_recording.hpp>
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <memory>

namespace
{
std::filesystem::path fixture_path()
{
    // CTest sets NOVA_TEST_FIXTURES to tests/fixtures/.
    const char *root = std::getenv("NOVA_TEST_FIXTURES");
    return std::filesystem::path{root ? root : "."} / "test_data.aedat4";
}
} // namespace

TEST(ReaderFlow, FixtureExists)
{
    ASSERT_TRUE(std::filesystem::exists(fixture_path())) << "Missing fixture: " << fixture_path();
}

TEST(ReaderFlow, OpenFixtureAndReadEvents)
{
    auto recording = std::make_unique<dv::io::MonoCameraRecording>(fixture_path().string());
    DVEventReader reader{std::move(recording)};

    ASSERT_TRUE(reader.isEventStreamAvailable());

    auto res = reader.getEventResolution();
    ASSERT_TRUE(res.has_value());
    EXPECT_GT(res->width, 0);
    EXPECT_GT(res->height, 0);

    EventData ed{};
    ed.set_camera_event_resolution(res->width, res->height);

    size_t total_events = 0;
    while (reader.isEventsRunning())
    {
        auto batch = reader.getNextEventBatch();
        if (!batch.has_value())
            break;
        for (const auto &e : *batch)
        {
            ed.write_evt_data(e);
            ++total_events;
        }
    }

    EXPECT_GT(total_events, 0u) << "fixture produced no events";

    ed.lock_data_vectors();
    const auto &vec = ed.get_evt_vector_ref();
    ASSERT_EQ(vec.size(), total_events);

    // Relative timestamps must be monotonically non-decreasing.
    for (size_t i = 1; i < vec.size(); ++i)
    {
        ASSERT_LE(vec[i - 1].z, vec[i].z) << "out-of-order timestamp at index " << i;
    }
    EXPECT_FLOAT_EQ(vec[0].z, 0.0f);
    ed.unlock_data_vectors();

    EXPECT_GE(ed.get_earliest_evt_timestamp(), 0);
}

TEST(ReaderFlow, NonExistentFileThrows)
{
    EXPECT_ANY_THROW({
        auto r = std::make_unique<dv::io::MonoCameraRecording>("this-file-does-not-exist.aedat4");
        DVEventReader reader{std::move(r)};
    });
}
