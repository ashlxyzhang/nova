#pragma once

#ifndef SCRUBBER_HH
#define SCRUBBER_HH

#include "pch.hh"

#include "EventData.hh"

// this class will handle all things to do with scrubbing and window size of particles.
// since at the end of the day, everything is related to values in an vector,
// we need a way to easily switch between time and event domains
class Scrubber 
{
    private:
        EventData *event_data = nullptr;

        float lower_time = std::numeric_limits<float>::max();
        float current_time = 0.0f;
        float upper_time = std::numeric_limits<float>::min();

        std::size_t lower_index = std::numeric_limits<std::size_t>::max();
        std::size_t current_index = 0;
        std::size_t upper_index = std::numeric_limits<std::size_t>::min();

    public:
        Scrubber(EventData *event_data) : event_data(event_data)
        {
            // TODO, set scrub to midpoint, 
            // and lower and upper to min and max to data in event
        }
};

#endif