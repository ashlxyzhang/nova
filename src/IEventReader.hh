#pragma once
#ifndef IEVENTREADER_HH
#define IEVENTREADER_HH

#include "EventData.hh"
#include <opencv2/core.hpp>
#include <optional>
#include <vector>

/**
 * @brief Backend-agnostic interface for reading event camera data.
 *        Implementations exist for dv-processing (.aedat4, inivation cameras)
 *        and OpenEB/Metavision (.raw, .dat, Prophesee cameras).
 */
class IEventReader
{
    public:
        virtual ~IEventReader() = default;

        /** Returns true if this source can ever provide event data. */
        virtual bool isEventStreamAvailable() const = 0;

        /** Returns true if this source can ever provide APS frame data. */
        virtual bool isFrameStreamAvailable() const = 0;

        /** Returns true while the event stream has more data (file) or is live (camera). */
        virtual bool isEventsRunning() const = 0;

        /** Returns true while the frame stream has more data (file) or is live (camera). */
        virtual bool isFramesRunning() const = 0;

        /** Returns the event sensor resolution, or nullopt if unknown. */
        virtual std::optional<cv::Size> getEventResolution() const = 0;

        /** Returns the APS frame sensor resolution, or nullopt if unknown. */
        virtual std::optional<cv::Size> getFrameResolution() const = 0;

        /**
         * @brief Gets the next batch of events.
         * @return nullopt if the stream has ended; otherwise a (possibly empty) vector of events.
         */
        virtual std::optional<std::vector<EventData::EventDatum>> getNextEventBatch() = 0;

        /**
         * @brief Gets the next APS frame in native BGR format.
         *        Callers are responsible for any color-space conversion needed for display.
         * @return nullopt if no frame is available or the stream has ended.
         */
        virtual std::optional<EventData::FrameDatum> getNextFrame() = 0;
};

#endif // IEVENTREADER_HH
