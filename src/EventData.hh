#pragma once

#ifndef EVENTDATA_HH
#define EVENTDATA_HH

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <iomanip>
#include <iterator>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#include <boost/iostreams/device/mapped_file.hpp>
#include <set>

// From previous NOVA source code
// Used for timestamp comparisons of event data
inline bool event_less_vec4_t(const glm::vec4 &a, const glm::vec4 &b)
{
    return a.z < b.z;
}

// From previous NOVA source code
// Used for timestamp comparisons of frame data
inline bool frame_less_vec4_t(const std::pair<cv::Mat, float> &a, const std::pair<cv::Mat, float> &b)
{
    return a.second < b.second;
}

class EventData
{
    public:
        class MappedEventBuffer
        {
            public:
                using value_type = glm::vec4;
                using iterator = value_type *;
                using const_iterator = const value_type *;

                MappedEventBuffer()
                {
                    std::ostringstream oss;
                    oss << "nova_evt_buffer_" << std::hex << std::hash<std::thread::id>{}(std::this_thread::get_id())
                        << "_" << reinterpret_cast<std::uintptr_t>(this) << ".bin";
                    file_path_ = std::filesystem::current_path() / oss.str();
                    remap(kInitialCapacity);
                }

                ~MappedEventBuffer()
                {
                    mapped_file_.close();
                    if (!file_path_.empty())
                    {
                        std::error_code ec;
                        std::filesystem::remove(file_path_, ec);
                    }
                }

                MappedEventBuffer(const MappedEventBuffer &) = delete;
                MappedEventBuffer &operator=(const MappedEventBuffer &) = delete;
                MappedEventBuffer(MappedEventBuffer &&) noexcept = delete;
                MappedEventBuffer &operator=(MappedEventBuffer &&) noexcept = delete;

                void push_back(const value_type &value)
                {
                    ensure_capacity(size_ + 1);
                    ptr()[size_] = value;
                    ++size_;
                }

                [[nodiscard]] bool empty() const
                {
                    return size_ == 0;
                }

                [[nodiscard]] std::size_t size() const
                {
                    return size_;
                }

                [[nodiscard]] std::size_t capacity() const
                {
                    return capacity_;
                }

                [[nodiscard]] std::size_t max_size() const
                {
                    return capacity_;
                }

                void clear()
                {
                    size_ = 0;
                }

                value_type &operator[](std::size_t index)
                {
                    return ptr()[index];
                }

                const value_type &operator[](std::size_t index) const
                {
                    return ptr()[index];
                }

                value_type &back()
                {
                    return ptr()[size_ - 1];
                }

                const value_type &back() const
                {
                    return ptr()[size_ - 1];
                }

                iterator begin()
                {
                    return ptr();
                }

                iterator end()
                {
                    return ptr() + size_;
                }

                const_iterator begin() const
                {
                    return ptr();
                }

                const_iterator end() const
                {
                    return ptr() + size_;
                }

                const_iterator cbegin() const
                {
                    return ptr();
                }

                const_iterator cend() const
                {
                    return ptr() + size_;
                }

                value_type *data()
                {
                    return ptr();
                }

                const value_type *data() const
                {
                    return ptr();
                }

            private:
                static constexpr std::size_t kInitialCapacity{4096};

                boost::iostreams::mapped_file mapped_file_;
                std::filesystem::path file_path_;
                std::size_t size_{0};
                std::size_t capacity_{0};

                void ensure_capacity(std::size_t min_capacity)
                {
                    if (min_capacity <= capacity_)
                    {
                        return;
                    }

                    std::size_t new_capacity = capacity_ == 0 ? kInitialCapacity : capacity_;
                    while (new_capacity < min_capacity)
                    {
                        new_capacity *= 2;
                    }

                    remap(new_capacity);
                }

                void remap(std::size_t new_capacity)
                {
                    const std::size_t bytes = new_capacity * sizeof(value_type);

                    mapped_file_.close();

                    if (bytes == 0)
                    {
                        capacity_ = 0;
                        return;
                    }

                    {
                        std::fstream file(file_path_, std::ios::binary | std::ios::in | std::ios::out);
                        if (!file)
                        {
                            std::ofstream create(file_path_, std::ios::binary | std::ios::out | std::ios::trunc);
                            create.close();
                            file.open(file_path_, std::ios::binary | std::ios::in | std::ios::out);
                        }

                        file.seekp(static_cast<std::streamoff>(bytes) - 1, std::ios::beg);
                        char zero = 0;
                        file.write(&zero, 1);
                    }

                    boost::iostreams::mapped_file_params params;
                    params.path = file_path_.string();
                    params.mode = std::ios_base::in | std::ios_base::out;
                    params.length = bytes;
                    mapped_file_.open(params);
                    if (!mapped_file_.is_open())
                    {
                        throw std::runtime_error("Failed to open mapped file for EventData buffer.");
                    }
                    capacity_ = new_capacity;
                }

                void erase_front(std::size_t count)
                {
                    if (count == 0 || count > size_)
                    {
                        return;
                    }

                    value_type *base_ptr = ptr();
                    std::memmove(base_ptr, base_ptr + count, (size_ - count) * sizeof(value_type));
                    size_ -= count;
                }

                value_type *ptr()
                {
                    return capacity_ == 0 ? nullptr : reinterpret_cast<value_type *>(mapped_file_.data());
                }

                const value_type *ptr() const
                {
                    return capacity_ == 0 ? nullptr : reinterpret_cast<const value_type *>(mapped_file_.data());
                }
        };

        // Internal structs
        // Represents single event datum
        struct EventDatum
        {
                int32_t x;
                int32_t y;
                int64_t timestamp;
                uint8_t polarity;
        };

        // Represents a single frame datum
        struct FrameDatum
        {
                cv::Mat frameData;
                int64_t timestamp;
        };

        // Member variables
    private:
        MappedEventBuffer evt_data_vector_relative;
        std::vector<std::pair<cv::Mat, float>> frame_data_vector_relative;

        // Earliest event/frame timestamps
        int64_t evt_data_earliest_timestamp{-1};

        int64_t evt_data_latest_timestamp{-1};
        int64_t frame_data_latest_timestamp{-1};

        // Set camera resolution
        int32_t camera_event_width{};
        int32_t camera_event_height{};

        int32_t camera_frame_width{};
        int32_t camera_frame_height{};

        std::recursive_mutex evt_lock;

    public:
        /**
         * @brief Default constructor for event data.
         */
        EventData()
            : evt_data_vector_relative{}, frame_data_vector_relative{}, evt_data_earliest_timestamp{-1},
              evt_data_latest_timestamp{-1}, frame_data_latest_timestamp{-1}, camera_event_width{},
              camera_event_height{}, camera_frame_width{}, camera_frame_height{}, evt_lock{}
        {
        }

        /**
         * @brief Clears everything
         */
        void clear()
        {
            std::unique_lock<std::recursive_mutex> evt_lock_ul{evt_lock};

            // Clear ref vectors
            evt_data_vector_relative.clear();
            frame_data_vector_relative.clear();

            evt_data_earliest_timestamp = -1;

            evt_data_latest_timestamp = -1;
            frame_data_latest_timestamp = -1;

            camera_event_width = 0;
            camera_event_height = 0;

            camera_frame_width = 0;
            camera_frame_height = 0;

            evt_lock_ul.unlock();
        }

        /**
         * @brief Locks event data vectors. If a thread calls get_*_vector_ref functions and uses the returned
         *        reference to vectors, the thread must call this. Any use of the reference vector must be inside a
         *        critical section for thread-safety.
         */
        void lock_data_vectors()
        {
            evt_lock.lock();
        }

        /**
         * @brief Unlocks event data vectors. If a thread calls get_*_vector_ref functions and uses the returned
         *        reference to vectors, the thread must call this after it is done using the data vectors.
         *        Any use of the reference vector must be inside a critical section for thread-safety.
         */
        void unlock_data_vectors()
        {
            evt_lock.unlock();
        }

        /**
         * Sets camera resolution for event data.
         * @param width Width of camera.
         * @param height Height of camera.
         */
        void set_camera_event_resolution(int32_t width, int32_t height)
        {
            std::unique_lock<std::recursive_mutex> evt_lock_ul{evt_lock};
            camera_event_width = width;
            camera_event_height = height;
            evt_lock_ul.unlock();
        }

        /**
         * Sets camera resolution for frame data.
         * @param width Width of camera.
         * @param height Height of camera.
         */
        void set_camera_frame_resolution(int32_t width, int32_t height)
        {
            std::unique_lock<std::recursive_mutex> evt_lock_ul{evt_lock};
            camera_frame_width = width;
            camera_frame_height = height;
            evt_lock_ul.unlock();
        }

        /**
         * Gets event camera resolution.
         * @return event camera resolution as glm::vec2 (width, height).
         */
        glm::vec2 get_camera_event_resolution()
        {
            std::unique_lock<std::recursive_mutex> evt_lock_ul{evt_lock};
            glm::vec2 camera_res{static_cast<float>(camera_event_width), static_cast<float>(camera_event_height)};
            evt_lock_ul.unlock();
            return camera_res;
        }

        /**
         * Gets frame camera resolution.
         * @return frame camera resolution as glm::vec2 (width, height).
         */
        glm::vec2 get_camera_frame_resolution()
        {
            std::unique_lock<std::recursive_mutex> evt_lock_ul{evt_lock};
            glm::vec2 camera_res{static_cast<float>(camera_frame_width), static_cast<float>(camera_frame_height)};
            evt_lock_ul.unlock();
            return camera_res;
        }

        /**
         * @brief Inserts event data into the event data vector with relative timestamps (absolute timestamp - absolute
         * earliest timestamp). Following documentation of the AEDAT formats
         * (https://docs.inivation.com/_static/inivation-docs_2025-08-05.pdf page 163), data is assumed to be read in as
         * monotonically increasing timestamps. If a decreasing timestamp is detected, as per documentation, a camera
         * reset/syncronization is assumed where timestamps are reset to 0.
         * @param raw_evt_data Raw event data to add.
         */
        void write_evt_data(EventDatum raw_evt_data)
        {
            std::unique_lock<std::recursive_mutex> evt_lock_ul{evt_lock};

            constexpr size_t MAX_EVENT_BACKING_SIZE{
                100 * ((static_cast<size_t>(1) << 30) / (1 << 4))}; // Set 100 GB approximately

            // If this condition is met, then we have unordered data, assume camera reset, clear all previous data
            // Or maximum number of event data has been reached
            if ((raw_evt_data.timestamp < evt_data_latest_timestamp) || (evt_data_vector_relative.size() > MAX_EVENT_BACKING_SIZE))
            {
                // Reset assumed, timestamps are all back to zero, clear data
                evt_data_earliest_timestamp = -1;
                evt_data_latest_timestamp = -1;
                frame_data_latest_timestamp = -1;
                evt_data_vector_relative.clear();
                frame_data_vector_relative.clear();
            }

            // update earliest timestamp
            if (evt_data_vector_relative.empty())
            {
                evt_data_earliest_timestamp = raw_evt_data.timestamp;
            }

            float x{static_cast<float>(raw_evt_data.x)};
            float y{static_cast<float>(raw_evt_data.y)};
            float timestamp_relative{static_cast<float>(raw_evt_data.timestamp - evt_data_earliest_timestamp)};
            float polarity{static_cast<float>(raw_evt_data.polarity)};
            evt_data_vector_relative.push_back(glm::vec4{x, y, timestamp_relative, polarity});

            evt_data_latest_timestamp = raw_evt_data.timestamp;
        
            evt_lock_ul.unlock();
        }

        /**
         * @brief Inserts frame data into the frame data vector with relative timestamps (absolute timestamp - absolute
         * earliest timestamp). Following documentation of the AEDAT formats
         * (https://docs.inivation.com/_static/inivation-docs_2025-08-05.pdf page 163), data is assumed to be read in as
         * monotonically increasing timestamps. If a decreasing timestamp is detected, as per documentation, a camera
         * reset/syncronization is assumed where timestamps are reset to 0.
         * @param raw_frame_data Raw frame data to add.
         */
        void write_frame_data(FrameDatum raw_frame_data)
        {
            std::unique_lock<std::recursive_mutex> evt_lock_ul{evt_lock};

            constexpr size_t MAX_FRAME_SIZE{static_cast<size_t>(1) << 20}; // Set max to 1 million frames

            // If this condition is met, unordered frame assumes camera reset, 
            // or maximum number of frames met.
            if ((raw_frame_data.timestamp < frame_data_latest_timestamp) || (frame_data_vector_relative.size() > MAX_FRAME_SIZE))
            {
                // Reset assumed, timestamps are all back to zero, clear data
                evt_data_earliest_timestamp = -1;
                evt_data_latest_timestamp = -1;
                frame_data_latest_timestamp = -1;
                evt_data_vector_relative.clear();
                frame_data_vector_relative.clear();
            }

            // update earliest timestamp
            if (evt_data_vector_relative.empty()) // Need to normalize timestamps relative to event data, ignore until event data is in
            {
                return;
            }

            float timestamp_relative{static_cast<float>(raw_frame_data.timestamp - evt_data_earliest_timestamp)};

            frame_data_vector_relative.push_back(std::make_pair(raw_frame_data.frameData, timestamp_relative));

            frame_data_latest_timestamp = raw_frame_data.timestamp;
            
            evt_lock_ul.unlock();
        }

        /**
         * @brief Exposes event data with relative timestamp as a vector of glm::vec4.
         *        IMPORTANT: Caller must have called lock_data_vectors(). Call unlock_data_vectors()
         *        when done working with the data vectors.
         * @param relative If true, returns data with relative timestamps (earliest timestamp subtracted from all
         * timestamps). Otherwise regular timestamps.
         * @return const reference to internal event data vector.
         */
        const MappedEventBuffer &get_evt_vector_ref() const
        {
            return evt_data_vector_relative;
        }

        /**
         * @brief Exposes frame data with relative timestamp as a vector of pairs containing image data and
         * timestamp. IMPORTANT: Caller must have called lock_data_vectors(). Call unlock_data_vectors() when done
         * working with the data vectors.
         * @param relative If true, returns data with relative timestamps (earliest timestamp subtracted from all
         * timestamps). Otherwise regular timestamps.
         * @return const reference to internal frame data vector.
         */
        const std::vector<std::pair<cv::Mat, float>> &get_frame_vector_ref()
        {
            return frame_data_vector_relative;
        }

        /**
         * @brief Gets the earliest event timestamp.
         * @return earliest event data timestamp.
         */
        int64_t get_earliest_evt_timestamp()
        {
            std::unique_lock<std::recursive_mutex> evt_lock_ul{evt_lock};

            if (evt_data_vector_relative.empty())
            {
                evt_lock_ul.unlock();
                return -1; // No evt data
            }

            int64_t earliest_timestamp{evt_data_earliest_timestamp};

            evt_lock_ul.unlock();

            return earliest_timestamp;
        }

        /**
         * @brief Gets index of first event data in relative event data vector with timestamp that is equal or greater
         *        than provided timestamp.
         * @param timestamp Provided timestamp.
         * @return -1 if relative event data vector is empty, index otherwise.
         */
        int64_t get_event_index_from_relative_timestamp(float timestamp)
        {
            std::unique_lock<std::recursive_mutex> evt_lock_ul{evt_lock};

            if (evt_data_vector_relative.empty())
            {
                evt_lock_ul.unlock();
                return -1; // Vector is empty, return -1
            }

            glm::vec4 timestampVec4(0.0f, 0.0f, timestamp, 0.0f);

            // evt_data_vector_relative should be sorted by the way EventData class is setup.
            auto lb = std::lower_bound(evt_data_vector_relative.begin(), evt_data_vector_relative.end(), timestampVec4,
                                       event_less_vec4_t);
            if (lb == evt_data_vector_relative.end())
            {
                return -1; // Not found
            }
            int64_t ret_index{static_cast<int64_t>(std::distance(evt_data_vector_relative.begin(), lb))};

            evt_lock_ul.unlock();
            return ret_index;
        }

};

// Operator overloads necessary to use Datum internal structs as keys to multiset
// Define < operator for EventDatum
inline bool operator<(const EventData::EventDatum &left, const EventData::EventDatum &right)
{
    return left.timestamp < right.timestamp;
}

// Define < operator for FrameDatum
inline bool operator<(const EventData::FrameDatum &left, const EventData::FrameDatum &right)
{
    return left.timestamp < right.timestamp;
}

#endif // EVENTDATA_HH
