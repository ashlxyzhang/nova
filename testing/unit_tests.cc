#include "../src/EventData.hh"
#include "../src/ParameterStore.hh"
#include <gtest/gtest.h>
#include <iostream>

// Test settings and getting camera resolutions
TEST(EventData, CameraResolution)
{
    EventData test_ed{};
    test_ed.set_camera_event_resolution(1080, 1920);
    glm::vec2 evt_res{test_ed.get_camera_event_resolution()};

    EXPECT_EQ(evt_res, glm::vec2(1080, 1920)) << "Event Resolution Differs.";

    test_ed.set_camera_frame_resolution(1080, 1920);
    glm::vec2 frame_res{test_ed.get_camera_frame_resolution()};

    EXPECT_EQ(evt_res, glm::vec2(1080, 1920)) << "Frame Resolution Differs.";
}

// Test writing event data
TEST(EventData, write_evt_data)
{
    EventData test_ed{};
    test_ed.set_camera_event_resolution(1080, 1920);
    test_ed.set_camera_frame_resolution(1080, 1920);

    constexpr int32_t NUM_ELEMENTS{5};
    for (int32_t i{0}; i < NUM_ELEMENTS; ++i)
    {
        EventData::EventDatum evt_datum{.x = i, .y = i, .timestamp = i, .polarity = 0};
        test_ed.write_evt_data(evt_datum);
    }

    // Test event data is written in order
    {
        test_ed.lock_data_vectors();
        const auto &evt_data_vec{test_ed.get_evt_vector_ref()};
        ASSERT_EQ(evt_data_vec.size(), NUM_ELEMENTS) << "Internal event data size mismatch.";
        for (int32_t i{0}; i < NUM_ELEMENTS; ++i)
        {
            glm::vec4 evt_datum{evt_data_vec[i]};
            EXPECT_EQ(evt_datum, glm::vec4(i, i, i, 0)) << "Out of order writing of event data at " << i;
        }

        test_ed.unlock_data_vectors();
    }

    // Test event data timestamps are relative
    test_ed.clear();
    for (int32_t i{0}; i < NUM_ELEMENTS; ++i)
    {
        EventData::EventDatum evt_datum{.x = i, .y = i, .timestamp = i + 1, .polarity = 0};
        test_ed.write_evt_data(evt_datum);
    }
    {
        test_ed.lock_data_vectors();
        const auto &evt_data_vec{test_ed.get_evt_vector_ref()};
        ASSERT_EQ(evt_data_vec.size(), NUM_ELEMENTS)
            << "Internal event data size mismatch when testing relative timestamps.";
        for (int32_t i{0}; i < NUM_ELEMENTS; ++i)
        {
            glm::vec4 evt_datum{evt_data_vec[i]};
            EXPECT_EQ(evt_datum, glm::vec4(i, i, i, 0))
                << "Out of order writing of event data at " << i << " when testing relative timestamps.";
        }

        test_ed.unlock_data_vectors();
    }

    // Test camera reset
    EventData::EventDatum out_of_order_datum{.x = 0, .y = 0, .timestamp = 0, .polarity = 0};
    test_ed.write_evt_data(out_of_order_datum);
    {
        test_ed.lock_data_vectors();
        const auto &evt_data_vec{test_ed.get_evt_vector_ref()};
        EXPECT_EQ(evt_data_vec.size(), 1)
            << "Out of order writing of event data did not clear vector"; // Should have cleared vector before inserting
        test_ed.unlock_data_vectors();
    }

    // Write again after reset
    for (int32_t i{1}; i < NUM_ELEMENTS; ++i)
    {
        EventData::EventDatum evt_datum{.x = i, .y = i, .timestamp = i, .polarity = 0};
        test_ed.write_evt_data(evt_datum);
    }
    // Test event data is written in order
    {
        test_ed.lock_data_vectors();
        const auto &evt_data_vec{test_ed.get_evt_vector_ref()};
        ASSERT_EQ(evt_data_vec.size(), NUM_ELEMENTS) << "Internal event data size mismatch after reset.";
        for (int32_t i{0}; i < NUM_ELEMENTS; ++i)
        {
            glm::vec4 evt_datum{evt_data_vec[i]};
            EXPECT_EQ(evt_datum, glm::vec4(i, i, i, 0))
                << "Out of order writing of event data at " << i << " after reset.";
        }

        test_ed.unlock_data_vectors();
    }
}

// Test writing frame data
TEST(EventData, write_frame_data)
{
    EventData test_ed{};
    test_ed.set_camera_event_resolution(1080, 1920);
    test_ed.set_camera_frame_resolution(1080, 1920);

    // Frame data is relative to event data, there needs to be event data for frame data to populate, test this
    EventData::FrameDatum frame_datum{.frameData = cv::Mat(), .timestamp = 123};
    test_ed.write_frame_data(frame_datum);
    {
        test_ed.lock_data_vectors();
        const auto &frame_data_vec{test_ed.get_frame_vector_ref()};
        ASSERT_EQ(frame_data_vec.size(), 0) << "Frame data written even though no event data to be relative to.";
        test_ed.unlock_data_vectors();
    }

    EventData::EventDatum evt_datum{.x = 0, .y = 0, .timestamp = 0, .polarity = 0};
    test_ed.write_evt_data(evt_datum);

    constexpr int32_t NUM_ELEMENTS{5};
    for (int32_t i{0}; i < NUM_ELEMENTS; ++i)
    {
        EventData::FrameDatum frame_datum{.frameData = cv::Mat(), .timestamp = i};
        test_ed.write_frame_data(frame_datum);
    }

    // Test event data is written in order
    {
        test_ed.lock_data_vectors();
        const auto &frame_data_vec{test_ed.get_frame_vector_ref()};
        ASSERT_EQ(frame_data_vec.size(), NUM_ELEMENTS) << "Internal frame data size mismatch.";
        for (int32_t i{0}; i < NUM_ELEMENTS; ++i)
        {
            auto curr_timestamp{frame_data_vec[i].second};
            EXPECT_EQ(curr_timestamp, i) << "Out of order writing of frame data at " << i;
        }

        test_ed.unlock_data_vectors();
    }

    // Test frame data timestamps are relative
    test_ed.clear();
    EventData::EventDatum evt_datum_absolute{.x = 0, .y = 0, .timestamp = 1, .polarity = 0};
    test_ed.write_evt_data(evt_datum_absolute);
    for (int32_t i{0}; i < NUM_ELEMENTS; ++i)
    {
        EventData::FrameDatum frame_datum{.frameData = cv::Mat(), .timestamp = i + 1};
        test_ed.write_frame_data(frame_datum);
    }
    {
        test_ed.lock_data_vectors();
        const auto &frame_data_vec{test_ed.get_frame_vector_ref()};
        ASSERT_EQ(frame_data_vec.size(), NUM_ELEMENTS)
            << "Internal frame data size mismatch when testing relative timestamps.";
        for (int32_t i{0}; i < NUM_ELEMENTS; ++i)
        {
            auto curr_timestamp{frame_data_vec[i].second};
            EXPECT_EQ(curr_timestamp, i) << "Out of order writing of frame data at " << i
                                         << " when testing relative timestamps.";
        }

        test_ed.unlock_data_vectors();
    }

    // Test camera reset
    EventData::FrameDatum out_of_order_datum{.frameData = cv::Mat(), .timestamp = 0};
    test_ed.write_frame_data(out_of_order_datum); // Writing will fail since no event data exists after camera reset
    test_ed.write_evt_data(evt_datum);
    test_ed.write_frame_data(
        out_of_order_datum); // Write again so write succeeds with timestamps relative to event data
    {
        test_ed.lock_data_vectors();
        const auto &frame_data_vec{test_ed.get_frame_vector_ref()};
        EXPECT_EQ(frame_data_vec.size(), 1)
            << "Out of order writing of event data did not clear vector"; // Should have cleared vector before inserting
        test_ed.unlock_data_vectors();
    }

    // Write again after reset
    for (int32_t i{1}; i < NUM_ELEMENTS; ++i)
    {
        EventData::FrameDatum frame_datum{.frameData = cv::Mat(), .timestamp = i};
        test_ed.write_frame_data(frame_datum);
    }
    // Test event data is written in order
    {
        test_ed.lock_data_vectors();
        const auto &frame_data_vec{test_ed.get_frame_vector_ref()};
        ASSERT_EQ(frame_data_vec.size(), NUM_ELEMENTS) << "Internal frame data size mismatch after reset.";
        for (int32_t i{0}; i < NUM_ELEMENTS; ++i)
        {
            auto curr_timestamp{frame_data_vec[i].second};
            EXPECT_EQ(curr_timestamp, i) << "Out of order writing of frame data at " << i << " after reset.";
        }

        test_ed.unlock_data_vectors();
    }
}

// Test getting earliest event timestamp
TEST(EventData, get_earliest_evt_timestamp)
{
    EventData test_ed{};
    EventData::EventDatum evt_datum_absolute{.x = 0, .y = 0, .timestamp = 123, .polarity = 0};
    test_ed.write_evt_data(evt_datum_absolute);
    EXPECT_EQ(test_ed.get_earliest_evt_timestamp(), 123) << "Earliest event timestamp mismatch after writing normally.";

    EventData::EventDatum evt_datum_order{.x = 0, .y = 0, .timestamp = 125, .polarity = 0};
    test_ed.write_evt_data(evt_datum_absolute);
    EXPECT_EQ(test_ed.get_earliest_evt_timestamp(), 123)
        << "Earliest event timestamp mismatch after writing another datum.";

    EventData::EventDatum evt_datum_less{.x = 0, .y = 0, .timestamp = 3, .polarity = 0};
    test_ed.write_evt_data(evt_datum_less);
    EXPECT_EQ(test_ed.get_earliest_evt_timestamp(), 3)
        << "Earliest event timestamp mismatch after out of order writing.";
}

// Testing getting index from timestamp
TEST(EventData, get_event_index_from_timestamp)
{
    EventData test_ed{};

    // test on empty data
    int64_t empty_index = test_ed.get_event_index_from_relative_timestamp(0);
    EXPECT_EQ(empty_index, -1) << "Expected -1 for no existing event data.";

    EventData::EventDatum evt_datum1{.x = 0, .y = 0, .timestamp = 0, .polarity = 0};
    test_ed.write_evt_data(evt_datum1);

    {
        int64_t bad_index1 = test_ed.get_event_index_from_relative_timestamp(2);
        EXPECT_EQ(bad_index1, -1) << "Expected -1 for non-existent timestamp.";

        int64_t bad_index2 = test_ed.get_event_index_from_relative_timestamp(1);
        EXPECT_EQ(bad_index2, -1) << "Expected -1 for non-existent timestamp.";

        int64_t good_index1 = test_ed.get_event_index_from_relative_timestamp(0);
        EXPECT_EQ(good_index1, 0) << "Expected 0 for existing timestamp.";
    }

    EventData::EventDatum evt_datum2{.x = 0, .y = 0, .timestamp = 123, .polarity = 0};
    test_ed.write_evt_data(evt_datum2);

    EventData::EventDatum evt_datum3{.x = 0, .y = 0, .timestamp = 1002, .polarity = 0};
    test_ed.write_evt_data(evt_datum3);

    {
        int64_t good_index1 = test_ed.get_event_index_from_relative_timestamp(0);
        EXPECT_EQ(good_index1, 0) << "Expected 0 for existing timestamp.";

        int64_t good_index2 = test_ed.get_event_index_from_relative_timestamp(123);
        EXPECT_EQ(good_index2, 1) << "Expected 1 for existing timestamp.";

        int64_t good_index3 = test_ed.get_event_index_from_relative_timestamp(1002);
        EXPECT_EQ(good_index3, 2) << "Expected 2 for existing timestamp.";

        int64_t good_index4 = test_ed.get_event_index_from_relative_timestamp(500);
        EXPECT_EQ(good_index4, 2) << "Expected 2 for timestamp greater than provided timestamp.";

        int64_t bad_index1 = test_ed.get_event_index_from_relative_timestamp(10000);
        EXPECT_EQ(bad_index1, -1)
            << "Expected -1 for non-existent timestamp (no timestamp greater than or equal to provided exist).";
    }
}

// Testing methods of ParameterStore
TEST(ParameterStore, add_get_exists)
{
    ParameterStore test_param_store{};

    // Test basic adding and getting
    test_param_store.add("key1", 1);
    test_param_store.add("key2", std::string("value"));

    EXPECT_EQ(test_param_store.get<int32_t>("key1"), 1) << "ParameterStore did not get correct int value.";
    EXPECT_EQ(test_param_store.get<std::string>("key2"), std::string("value"))
        << "ParameterStore did not get correct string value.";

    // Test modifying existing key
    test_param_store.add("key1", 123);
    EXPECT_EQ(test_param_store.get<int32_t>("key1"), 123) << "Parameter store did not correctly modify value.";

    // Test exist function
    EXPECT_EQ(test_param_store.exists("key1"), true) << "Parameter store did not detect existing key.";
    EXPECT_EQ(test_param_store.exists("abc"), false) << "Parameter store detected non-existent key.";
}