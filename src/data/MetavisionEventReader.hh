#pragma once
#ifndef METAVISIONEVENTREADER_HH
#define METAVISIONEVENTREADER_HH

#include "data/IEventReader.hh"
#include <atomic>
#include <iostream>
#include <metavision/hal/facilities/i_erc_module.h>
#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/camera.h>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace nova {


/**
 * @brief IEventReader backed by the OpenEB/Metavision SDK.
 *        Supports .raw, .dat, and other Prophesee file formats,
 *        as well as live Prophesee cameras.
 *
 * Events are delivered via Metavision's callback mechanism and buffered
 * internally. getNextEventBatch() drains the buffer on each call.
 * APS frames are not currently supported (Prophesee .raw files are events-only).
 */
class MetavisionEventReader final : public IEventReader
{
    public:
        /// Tag type selecting the live-camera constructor. Empty serial = first available.
        struct LiveCamera { std::string serial; };

    private:
        std::optional<Metavision::Camera> camera_;
        bool started_{false};
        bool is_live_{false};
        std::atomic<bool> camera_stopped_{false};

        mutable std::mutex buffer_mutex_;
        std::vector<EventData::EventDatum> event_buffer_;

        int32_t width_{0};
        int32_t height_{0};

        void install_callbacks()
        {
            camera_->cd().add_callback([this](const Metavision::EventCD *begin, const Metavision::EventCD *end) {
                std::lock_guard<std::mutex> lock(buffer_mutex_);
                event_buffer_.reserve(event_buffer_.size() + static_cast<size_t>(end - begin));
                for (auto it = begin; it != end; ++it)
                {
                    event_buffer_.push_back({.x = static_cast<int32_t>(it->x),
                                             .y = static_cast<int32_t>(it->y),
                                             .timestamp = static_cast<int64_t>(it->t),
                                             .polarity = static_cast<uint8_t>(it->p)});
                }
            });

            camera_->add_status_change_callback([this](const Metavision::CameraStatus &status) {
                if (status == Metavision::CameraStatus::STOPPED)
                {
                    // Hold the lock so this flag becomes visible only after any
                    // in-flight CD callback has finished writing to event_buffer_.
                    std::lock_guard<std::mutex> lock(buffer_mutex_);
                    camera_stopped_ = true;
                }
            });
        }

    public:
        explicit MetavisionEventReader(const std::string &path)
        {
            Metavision::FileConfigHints hints;
            hints.real_time_playback(false); // Read as fast as possible

            camera_.emplace(Metavision::Camera::from_file(path, hints));

            width_ = camera_->geometry().get_width();
            height_ = camera_->geometry().get_height();

            install_callbacks();
        }

        explicit MetavisionEventReader(const LiveCamera &live)
        {
            is_live_ = true;

            if (live.serial.empty())
                camera_.emplace(Metavision::Camera::from_first_available());
            else
                camera_.emplace(Metavision::Camera::from_serial(live.serial));

            width_ = camera_->geometry().get_width();
            height_ = camera_->geometry().get_height();

            // Cap the sensor's outgoing event rate in hardware so the host is
            // never asked to consume more than it can process. Without this,
            // scene activity can trivially saturate the USB link and the
            // consumer falls perpetually behind.
            try
            {
                auto *erc = camera_->get_device().get_facility<Metavision::I_ErcModule>();
                if (erc)
                {
                    erc->set_cd_event_rate(2'000'000); // 5 Mev/s
                    erc->enable(true);
                }
            }
            catch (...)
            {
                std::cerr << "MetavisionEventReader: ERC module unavailable; event rate is uncapped." << std::endl;
            }

            install_callbacks();
        }

        ~MetavisionEventReader() override
        {
            if (started_ && !camera_stopped_)
            {
                camera_->stop();
            }
        }

        bool isEventStreamAvailable() const override
        {
            return true;
        }

        bool isFrameStreamAvailable() const override
        {
            return false;
        }

        bool isEventsRunning() const override
        {
            if (!started_)
                return true; // Not yet started; assume data is available
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            return !camera_stopped_ || !event_buffer_.empty();
        }

        bool isFramesRunning() const override
        {
            return false;
        }

        std::optional<cv::Size> getEventResolution() const override
        {
            if (width_ <= 0 || height_ <= 0)
                return std::nullopt;
            return cv::Size(width_, height_);
        }

        std::optional<cv::Size> getFrameResolution() const override
        {
            return std::nullopt;
        }

        std::optional<std::vector<EventData::EventDatum>> getNextEventBatch() override
        {
            if (!started_)
            {
                camera_->start();
                started_ = true;
            }

            std::lock_guard<std::mutex> lock(buffer_mutex_);

            if (camera_stopped_ && event_buffer_.empty())
                return std::nullopt; // EOF and buffer fully drained

            std::vector<EventData::EventDatum> result;
            std::swap(result, event_buffer_);
            return result; // may be empty while camera is still running
        }

        std::optional<EventData::FrameDatum> getNextFrame() override
        {
            return std::nullopt;
        }
};

#endif // METAVISIONEVENTREADER_HH

} // namespace nova
