#pragma once

#ifndef EVENTDATA_HH
#define EVENTDATA_HH

#include <deque>
#include <glm/glm.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

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
        // Internal structs
    public:
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
        std::vector<glm::vec4> evt_data_vector_relative;
        std::vector<std::pair<cv::Mat, float>> frame_data_vector_relative;

        // Earliest event/frame timestamps
        int64_t evt_data_earliest_timestamp{-1};
        int64_t frame_data_earliest_timestamp{-1};

        int64_t evt_data_latest_timestamp{-1};
        int64_t frame_data_latest_timestamp{-1};

        // Set camera resolution
        int32_t camera_event_width{};
        int32_t camera_event_height{};

        int32_t camera_frame_width{};
        int32_t camera_frame_height{};

        float max_element_percentage;  // What percentage of max size of vector should number of elements populate
        float cull_element_percentage; // What percentage of max size of vector should elements be culled to if they
                                       // exceed max_element_percentage

        std::recursive_mutex evt_lock;

    public:
        /**
         * @brief Default constructor for event data.
         */
        EventData()
            : evt_data_vector_relative{}, frame_data_vector_relative{}, evt_data_earliest_timestamp{-1},
              frame_data_earliest_timestamp{-1}, evt_data_latest_timestamp{-1}, frame_data_latest_timestamp{-1},
              camera_event_width{}, camera_event_height{}, camera_frame_width{}, camera_frame_height{},
              max_element_percentage{0.8f}, cull_element_percentage{0.5f}, evt_lock{}
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
            frame_data_earliest_timestamp = -1;

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

            // If this condition is met, then we only need to push back event data since it is ordered
            if (raw_evt_data.timestamp >= evt_data_latest_timestamp)
            {
                // update earliest timestamp
                if (evt_data_vector_relative.empty())
                {
                    evt_data_earliest_timestamp = raw_evt_data.timestamp;
                }

                // cull elements to avoid overflowing memory
                cull_elements(evt_data_vector_relative, max_element_percentage, cull_element_percentage);
                float x{static_cast<float>(raw_evt_data.x)};
                float y{static_cast<float>(raw_evt_data.y)};
                float timestamp_relative{static_cast<float>(raw_evt_data.timestamp - evt_data_earliest_timestamp)};
                float polarity{static_cast<float>(raw_evt_data.polarity)};
                evt_data_vector_relative.push_back(glm::vec4{x, y, timestamp_relative, polarity});

                
                evt_data_latest_timestamp = raw_evt_data.timestamp;
            }
            else
            {
                // Reset assumed, timestamps are all back to zero, clear data
                evt_data_earliest_timestamp = -1;
                frame_data_earliest_timestamp = -1;
                evt_data_vector_relative.clear();
                frame_data_vector_relative.clear();
            }
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

            // If this condition is met, then we only need to push back event data since it is ordered
            if (raw_frame_data.timestamp >= frame_data_latest_timestamp)
            {
                // update earliest timestamp
                if (frame_data_vector_relative.empty())
                {
                    frame_data_earliest_timestamp = raw_frame_data.timestamp;
                }

                // cull elements to avoid overflowing memory
                cull_elements(frame_data_vector_relative, max_element_percentage, cull_element_percentage);
                float timestamp_relative{static_cast<float>(raw_frame_data.timestamp - frame_data_earliest_timestamp)};
                frame_data_vector_relative.push_back(std::make_pair(raw_frame_data.frameData, timestamp_relative));

                
                frame_data_latest_timestamp = raw_frame_data.timestamp;
            }
            else
            {
                // Reset assumed, timestamps are all back to zero, clear data
                evt_data_earliest_timestamp = -1;
                frame_data_earliest_timestamp = -1;
                evt_data_vector_relative.clear();
                frame_data_vector_relative.clear();
            }
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
        const std::vector<glm::vec4> &get_evt_vector_ref()
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
         * @brief Gets the earliest frame timestamp.
         * @return earliest frame data timestamp.
         */
        int64_t get_earliest_frame_timestamp()
        {
            std::unique_lock<std::recursive_mutex> evt_lock_ul{evt_lock};

            if (frame_data_vector_relative.empty())
            {
                evt_lock_ul.unlock();
                return -1; // No frame data
            }

            int64_t timestamp{frame_data_earliest_timestamp};

            evt_lock_ul.unlock();

            return timestamp;
        }

        /**
         * @brief Gets index of first event data in relative event data vector with timestamp that is equal or greater
         *        than provided timestamp.
         * @param timestamp Provided timestamp.
         * @return -1 if relative event data vector is empty, index otherwise.
         */
        int64_t get_event_index_from_timestamp(float timestamp)
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
                return evt_data_vector_relative.size() - 1;
            }
            int64_t ret_index{static_cast<int64_t>(std::distance(evt_data_vector_relative.begin(), lb))};

            evt_lock_ul.unlock();
            return ret_index;
        }

        /**
         * @brief Gets index of first frame data in relative event data vector with timestamp that is equal or greater
         *        than provided timestamp.
         * @param timestamp Provided timestamp.
         * @return -1 if relative frame data vector is empty, index otherwise.
         */
        int64_t get_frame_index_from_timestamp(float timestamp)
        {
            std::unique_lock<std::recursive_mutex> evt_lock_ul{evt_lock};

            if (frame_data_vector_relative.empty())
            {
                evt_lock_ul.unlock();
                return -1; // Vector is empty, return -1
            }

            std::pair<cv::Mat, float> timestampPair{cv::Mat{}, timestamp};

            // frame_data_vector_relative should be sorted by the way EventData class is setup.
            auto lb = std::lower_bound(frame_data_vector_relative.begin(), frame_data_vector_relative.end(),
                                       timestampPair, frame_less_vec4_t);
            if (lb == frame_data_vector_relative.end())
            {
                return frame_data_vector_relative.size() - 1;
            }
            int64_t ret_index{static_cast<int64_t>(std::distance(frame_data_vector_relative.begin(), lb))};

            evt_lock_ul.unlock();
            return ret_index;
        }

        // Helper functions
    private:
        /**
         * @brief Memory management code. Must be called with appropriate locks.
         */
        template <typename T>
        bool cull_elements(std::vector<T> &vector_data, float max_percentage, float cull_percentage)
        {
            bool culled = false;
            size_t maxElements{vector_data.max_size()};
            if (vector_data.size() >=
                static_cast<size_t>(max_percentage *
                                    maxElements)) // If there are more than max_percentage max number of events
            {
                // Ensures upper bound is met no matter the condition
                while (vector_data.size() > static_cast<size_t>(cull_percentage * maxElements))
                {
                    vector_data.erase(
                        vector_data.begin(),
                        std::next(vector_data.begin(),
                                  static_cast<size_t>((max_percentage - cull_percentage) *
                                                      maxElements))); // Should bring number of elements down to
                                                                      // cull_percentage of max elements
                    culled = true;
                }
            }

            return culled;
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
