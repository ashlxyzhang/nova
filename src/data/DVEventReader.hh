#pragma once
#ifndef DVEVENTREADER_HH
#define DVEVENTREADER_HH

#include "data/IEventReader.hh"
#include <dv-processing/io/mono_camera_recording.hpp>

/**
 * @brief IEventReader backed by the dv-processing library.
 *        Supports .aedat4 files and inivation cameras (DAVIS, DVXplorer, etc.).
 */
class DVEventReader final : public IEventReader
{
        std::unique_ptr<dv::io::InputBase> reader_;

    public:
        explicit DVEventReader(std::unique_ptr<dv::io::InputBase> reader) : reader_{std::move(reader)}
        {
        }

        bool isEventStreamAvailable() const override
        {
            return reader_->isEventStreamAvailable();
        }

        bool isFrameStreamAvailable() const override
        {
            return reader_->isFrameStreamAvailable();
        }

        bool isEventsRunning() const override
        {
            return reader_->isRunning("events");
        }

        bool isFramesRunning() const override
        {
            return reader_->isRunning("frames");
        }

        std::optional<cv::Size> getEventResolution() const override
        {
            auto res = reader_->getEventResolution();
            if (!res.has_value())
                return std::nullopt;
            return cv::Size(res->width, res->height);
        }

        std::optional<cv::Size> getFrameResolution() const override
        {
            auto res = reader_->getFrameResolution();
            if (!res.has_value())
                return std::nullopt;
            return cv::Size(res->width, res->height);
        }

        std::optional<std::vector<EventData::EventDatum>> getNextEventBatch() override
        {
            if (!isEventStreamAvailable() || !isEventsRunning())
                return std::nullopt;

            auto events = reader_->getNextEventBatch();
            if (!events.has_value())
                return std::nullopt;

            std::vector<EventData::EventDatum> result;
            result.reserve(events->size());
            for (const auto &e : *events)
            {
                result.push_back({.x = static_cast<int32_t>(e.x()),
                                  .y = static_cast<int32_t>(e.y()),
                                  .timestamp = e.timestamp(),
                                  .polarity = static_cast<uint8_t>(e.polarity())});
            }
            return result;
        }

        std::optional<EventData::FrameDatum> getNextFrame() override
        {
            if (!isFrameStreamAvailable() || !isFramesRunning())
                return std::nullopt;

            auto frame = reader_->getNextFrame();
            if (!frame.has_value())
                return std::nullopt;

            // Return the native BGR image; DataAcquisition handles BGR->RGB for display
            return EventData::FrameDatum{.frameData = frame->image.clone(), .timestamp = frame->timestamp};
        }
};

#endif // DVEVENTREADER_HH
