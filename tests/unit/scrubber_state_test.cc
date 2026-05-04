// Tests for Scrubber::State pure logic (no GPU required).
// The Scrubber class itself owns an SDL_GPUDevice, but State is a plain struct
// whose update()/step_forward()/update_bounds() operate on an EventData instance.
#include <nova/nova.hh>
#include <gtest/gtest.h>

using namespace nova;

static void fill_events(EventData &ed, int32_t n, int64_t step_us = 1000)
{
    for (int32_t i{0}; i < n; ++i)
    {
        ed.write_evt_data({.x = 0, .y = 0, .timestamp = i * step_us, .polarity = 0});
    }
}

TEST(ScrubberState, UpdateBoundsOnEmptyClearsState)
{
    EventData ed{};
    Scrubber::State s;
    s.current_index = 42;
    s.max_index = 100;

    s.update_bounds(ed);

    EXPECT_EQ(s.current_index, 0u);
    EXPECT_EQ(s.max_index, 0u);
    EXPECT_FLOAT_EQ(s.max_time, 0.0f);
}

TEST(ScrubberState, UpdateBoundsReflectsEventCount)
{
    EventData ed{};
    fill_events(ed, 10);

    Scrubber::State s;
    s.update_bounds(ed);

    EXPECT_EQ(s.min_index, 0u);
    EXPECT_EQ(s.max_index, 9u);
    EXPECT_FLOAT_EQ(s.min_time, 0.0f);
    EXPECT_FLOAT_EQ(s.max_time, 9000.0f);
}

TEST(ScrubberState, PlayingAdvancesCurrentTimeByStep)
{
    EventData ed{};
    fill_events(ed, 100);

    Scrubber::State s;
    s.type = Scrubber::Type::TIME;
    s.mode = Scrubber::Mode::PLAYING;
    s.time_step = 1000.0f;
    s.time_window = 500.0f;
    s.current_time = 0.0f;

    s.update(ed);
    EXPECT_FLOAT_EQ(s.current_time, 1000.0f);

    s.update(ed);
    EXPECT_FLOAT_EQ(s.current_time, 2000.0f);
}

TEST(ScrubberState, LatestModeSnapsToMaxTime)
{
    EventData ed{};
    fill_events(ed, 50);

    Scrubber::State s;
    s.type = Scrubber::Type::TIME;
    s.mode = Scrubber::Mode::LATEST;
    s.current_time = 0.0f;

    s.update(ed);

    EXPECT_FLOAT_EQ(s.current_time, s.max_time);
}

TEST(ScrubberState, PausedClampsCurrentTimeInRange)
{
    EventData ed{};
    fill_events(ed, 10);

    Scrubber::State s;
    s.type = Scrubber::Type::TIME;
    s.mode = Scrubber::Mode::PAUSED;
    s.current_time = 1e9f; // Way past max_time.

    s.update(ed);

    EXPECT_LE(s.current_time, s.max_time);
    EXPECT_GE(s.current_time, s.min_time);
}
