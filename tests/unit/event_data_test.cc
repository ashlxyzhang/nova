#include "data/EventData.hh"
#include <gtest/gtest.h>

TEST(EventData, CameraResolution)
{
    EventData ed{};
    ed.set_camera_event_resolution(1080, 1920);
    EXPECT_EQ(ed.get_camera_event_resolution(), glm::vec2(1080, 1920));

    ed.set_camera_frame_resolution(640, 480);
    EXPECT_EQ(ed.get_camera_frame_resolution(), glm::vec2(640, 480));
}

TEST(EventData, WriteAndOrderEvents)
{
    EventData ed{};
    constexpr int32_t N{5};
    for (int32_t i{0}; i < N; ++i)
    {
        ed.write_evt_data({.x = i, .y = i, .timestamp = i, .polarity = 0});
    }

    ed.lock_data_vectors();
    const auto &vec = ed.get_evt_vector_ref();
    ASSERT_EQ(vec.size(), static_cast<size_t>(N));
    for (int32_t i{0}; i < N; ++i)
    {
        EXPECT_EQ(vec[i], glm::vec4(i, i, i, 0)) << "out of order at " << i;
    }
    ed.unlock_data_vectors();
}

TEST(EventData, RelativeTimestampsAreZeroBased)
{
    EventData ed{};
    constexpr int32_t N{5};
    for (int32_t i{0}; i < N; ++i)
    {
        // Absolute timestamps start at 100 -- relative should start at 0.
        ed.write_evt_data({.x = i, .y = i, .timestamp = 100 + i, .polarity = 0});
    }

    ed.lock_data_vectors();
    const auto &vec = ed.get_evt_vector_ref();
    ASSERT_EQ(vec.size(), static_cast<size_t>(N));
    EXPECT_FLOAT_EQ(vec[0].z, 0.0f);
    EXPECT_FLOAT_EQ(vec[N - 1].z, static_cast<float>(N - 1));
    ed.unlock_data_vectors();

    EXPECT_EQ(ed.get_earliest_evt_timestamp(), 100);
}

TEST(EventData, OutOfOrderTimestampResetsBuffer)
{
    EventData ed{};
    for (int32_t i{0}; i < 5; ++i)
    {
        ed.write_evt_data({.x = i, .y = i, .timestamp = 100 + i, .polarity = 0});
    }

    // Lower-than-latest timestamp triggers assumed camera reset.
    ed.write_evt_data({.x = 0, .y = 0, .timestamp = 0, .polarity = 0});

    ed.lock_data_vectors();
    EXPECT_EQ(ed.get_evt_vector_ref().size(), 1u);
    ed.unlock_data_vectors();
}

TEST(EventData, GetEventIndexFromRelativeTimestamp)
{
    EventData ed{};

    // Empty buffer
    EXPECT_EQ(ed.get_event_index_from_relative_timestamp(0), -1);

    ed.write_evt_data({.x = 0, .y = 0, .timestamp = 0, .polarity = 0});
    EXPECT_EQ(ed.get_event_index_from_relative_timestamp(0), 0);
    EXPECT_EQ(ed.get_event_index_from_relative_timestamp(2), -1);

    ed.write_evt_data({.x = 0, .y = 0, .timestamp = 123, .polarity = 0});
    ed.write_evt_data({.x = 0, .y = 0, .timestamp = 1000, .polarity = 0});

    EXPECT_EQ(ed.get_event_index_from_relative_timestamp(0), 0);
    EXPECT_EQ(ed.get_event_index_from_relative_timestamp(123), 1);
    // Between known timestamps: lower_bound returns first element >= value.
    EXPECT_EQ(ed.get_event_index_from_relative_timestamp(124), 2);
    EXPECT_EQ(ed.get_event_index_from_relative_timestamp(1000), 2);
    EXPECT_EQ(ed.get_event_index_from_relative_timestamp(10000), -1);
}

TEST(EventData, ClearEmptiesBuffers)
{
    EventData ed{};
    ed.set_camera_event_resolution(100, 100);
    for (int32_t i{0}; i < 3; ++i)
    {
        ed.write_evt_data({.x = i, .y = i, .timestamp = i, .polarity = 0});
    }

    ed.clear();

    ed.lock_data_vectors();
    EXPECT_EQ(ed.get_evt_vector_ref().size(), 0u);
    ed.unlock_data_vectors();

    EXPECT_EQ(ed.get_earliest_evt_timestamp(), -1);
    EXPECT_EQ(ed.get_event_index_from_relative_timestamp(0), -1);
}
