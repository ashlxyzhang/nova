#include "../src/DataAcquisition.hh"
#include "../src/DataWriter.hh"
#include "../src/EventData.hh"
#include "../src/ParameterStore.hh"
#include <gtest/gtest.h>
#include <iostream>

// Function to check order of events.
// Returns index of mismatch or -1 if no mismatch.
int32_t check_order_events(EventData &evt_data)
{
    evt_data.lock_data_vectors();
    const auto &evt_vector{evt_data.get_evt_vector_ref()};

    for (int32_t i{1}; i < evt_vector.size(); ++i)
    {
        // Out of order timestamps detected in event data
        if (evt_vector[i - 1].z > evt_vector[i].z)
        {
            return i;
        }
    }
    evt_data.unlock_data_vectors();
    return -1;
}

// Function to check order of frames.
// Returns index of mismatch or -1 if no mismatch.
int32_t check_order_frames(EventData &evt_data)
{
    evt_data.lock_data_vectors();

    const auto &frame_vector{evt_data.get_frame_vector_ref()};

    for (int32_t i{1}; i < frame_vector.size(); ++i)
    {
        // Out of order timestamps detected in event data
        if (frame_vector[i - 1].second > frame_vector[i].second)
        {
            return i;
        }
    }

    evt_data.unlock_data_vectors();
    return -1;
}

// Function to check equality of events.
// Returns index of mismatch or -1 if no mismatch.
int32_t check_event_data_equality(EventData &evt_data1, EventData &evt_data2)
{
    evt_data1.lock_data_vectors();
    evt_data2.lock_data_vectors();
    const auto &evt_vector1{evt_data1.get_evt_vector_ref()};
    const auto &evt_vector2{evt_data2.get_evt_vector_ref()};

    for (int32_t i{0}; i < evt_vector1.size(); ++i)
    {
        if (evt_vector1[i] != evt_vector2[i])
        {
            return i;
        }
    }
    evt_data1.unlock_data_vectors();
    evt_data2.unlock_data_vectors();
    return -1;
}

// Function to check equality of frames.
// Returns index of mismatch or -1 if no mismatch.
int32_t check_frame_data_equality(EventData &evt_data1, EventData &evt_data2)
{
    evt_data1.lock_data_vectors();
    evt_data2.lock_data_vectors();
    const auto &frame_vector1{evt_data1.get_frame_vector_ref()};
    const auto &frame_vector2{evt_data2.get_frame_vector_ref()};

    for (int32_t i{0}; i < frame_vector1.size(); ++i)
    {
        // Check equality of cv matrices
        auto rows{(frame_vector1[i].first).rows};
        auto cols{(frame_vector1[i].first).cols};
        bool unequal{false};
        for (int32_t row{0}; row < rows; ++row)
        {
            for (int32_t col{0}; col < cols; ++col)
            {
                if (frame_vector1[i].first.at<char>(row, col) != frame_vector2[i].first.at<char>(row, col))
                {
                    unequal = true;
                    break;
                }
            }
            if (unequal)
            {
                break;
            }
        }

        // Unequal condition met
        if (unequal || frame_vector1[i].second != frame_vector2[i].second)
        {
            return i;
        }
    }
    evt_data1.unlock_data_vectors();
    evt_data2.unlock_data_vectors();
    return -1;
}

TEST(DataAcquisition, reading)
{
    DataAcquisition data_acq{};
    ParameterStore param_store{};
    EventData evt_data{};
    DataWriter data_writer{};

    param_store.add("pop_up_err_str", "");

    ASSERT_EQ(data_acq.init_file_reader("../testing/test_data.aedat4", param_store), true)
        << "Failed to initialize file for reading: " << param_store.get<std::string>("pop_up_err_str");

    while (data_acq.get_batch_evt_data(evt_data, param_store, data_writer, 1.0f) ||
           data_acq.get_batch_frame_data(evt_data, param_store, data_writer))
    {
    }

    // Check order of timestamps, by aedat standard, should be ordered.
    int32_t evt_ordered{check_order_events(evt_data)};
    EXPECT_EQ(evt_ordered, -1) << "Events are out of order at: " << evt_ordered;
    int32_t frame_ordered{check_order_frames(evt_data)};
    EXPECT_EQ(frame_ordered, -1) << "Frames are out of order at: " << frame_ordered;
}

TEST(DataWriter, writing)
{
    DataAcquisition data_acq{};
    ParameterStore param_store{};
    EventData evt_data{};
    DataWriter data_writer{};

    param_store.add("pop_up_err_str", "");

    // Initialize file for reading
    ASSERT_EQ(data_acq.init_file_reader("../testing/test_data.aedat4", param_store), true)
        << "Failed to initialize file for reading: " << param_store.get<std::string>("pop_up_err_str");

    // Initialize data writer to write to test_data_out.aedat4
    ASSERT_EQ(data_writer.init_data_writer("../testing/test_data_out.aedat4", data_acq.get_camera_event_width(),
                                           data_acq.get_camera_event_height(), data_acq.get_camera_frame_width(),
                                           data_acq.get_camera_frame_height(), true, true, param_store),
              true)
        << "Failed to initialize data writer: " << param_store.get<std::string>("pop_up_err_str");

    // Will get data and write it at the same time
    while (data_acq.get_batch_evt_data(evt_data, param_store, data_writer, 1.0f) ||
           data_acq.get_batch_frame_data(evt_data, param_store, data_writer))
    {
    }

    // Check order of timestamps, by aedat standard, should be ordered.
    int32_t evt_ordered{check_order_events(evt_data)};
    EXPECT_EQ(evt_ordered, -1) << "Events are out of order at: " << evt_ordered;
    int32_t frame_ordered{check_order_frames(evt_data)};
    EXPECT_EQ(frame_ordered, -1) << "Frames are out of order at: " << frame_ordered;

    while (data_writer.write_frame_data(param_store) || data_writer.write_event_store(param_store))
    {
    }

    // Clear data writer to flush io
    data_writer.clear();

    // Check that newly written file's data matches original file
    DataAcquisition data_acq_out{};
    EventData evt_data_out{};

    // Initialize output file for reading
    ASSERT_EQ(data_acq_out.init_file_reader("../testing/test_data_out.aedat4", param_store), true)
        << "Failed to initialize file output for reading: " << param_store.get<std::string>("pop_up_err_str");

    // Will get data and write it at the same time
    while (data_acq_out.get_batch_evt_data(evt_data_out, param_store, data_writer, 1.0f) ||
           data_acq_out.get_batch_frame_data(evt_data_out, param_store, data_writer))
    {
    }

    // Check order of timestamps, by aedat standard, should be ordered.
    int32_t evt_ordered_out{check_order_events(evt_data_out)};
    EXPECT_EQ(evt_ordered_out, -1) << "Events are out of order at: " << evt_ordered_out;
    int32_t frame_ordered_out{check_order_frames(evt_data_out)};
    EXPECT_EQ(frame_ordered_out, -1) << "Frames are out of order at: " << frame_ordered_out;

    // Check equality of written data with original data
    int32_t evt_equality{check_event_data_equality(evt_data, evt_data_out)};
    EXPECT_EQ(evt_equality, -1) << "Unequal events at: " << evt_equality;
    int32_t frame_equality{check_frame_data_equality(evt_data, evt_data_out)};
    EXPECT_EQ(frame_equality, -1) << "Unequal frames at: " << frame_equality;
}