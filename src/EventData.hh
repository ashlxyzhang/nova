#pragma once

#ifndef EVENTDATA_HH
#define EVENTDATA_HH

#include <vector>
#include <deque>
#include <glm/glm.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <set>




class EventData
{
	

	public:

        // Represents single event data
	    struct EventDatum
	    {
                int32_t x;
                int32_t y;
                int64_t timestamp;
                uint8_t polarity;
	    };

	    struct FrameDatum
	    {
                cv::Mat frameData;
                int64_t timestamp;
	    };

		/**
		 * @brief Default constructor for event data.
		 */
        EventData() : evt_data{}, frame_data{}
		{

		}

        /**
         * @brief Inserts event data into internel ordered set of event data.
         */
        void write_evt_data(EventDatum raw_evt_data)
        {
            evt_data.insert(raw_evt_data);
        }

        /**
         * @brief Inserts frame data into internal ordered set of frame data.
         */
        void write_frame_data(FrameDatum raw_frame_data)
        {
            frame_data.insert(raw_frame_data);

        }

        /**
         * @brief Exposes event data with absolute timestamp as a vector of glm::vec4.
         */
        std::vector<glm::vec4> get_evt_vector_absolute()
        {
            std::vector<glm::vec4> evt_vector{};
            cull_elements(evt_vector, evt_data, 0.8f, 0.5f);
            for (const EventDatum &evt_el : evt_data)
            {
                float x{static_cast<float>(evt_el.x)};
                float y{static_cast<float>(evt_el.y)};
                float timestamp{static_cast<float>(evt_el.timestamp)};
                float polarity{static_cast<float>(evt_el.polarity)};
                evt_vector.push_back(glm::vec4{x, y, timestamp, polarity});
            }

            return evt_vector;
        }

        /**
         * @brief Exposes event data with relative timestamp as a vector of glm::vec4.
         */
        std::vector<glm::vec4> get_evt_vector_relative()
        {
            std::vector<glm::vec4> evt_vector{};
            cull_elements(evt_vector, evt_data, 0.8f, 0.5f);
            for (const EventDatum &evt_el : evt_data)
            {
                float x{static_cast<float>(evt_el.x)};
                float y{static_cast<float>(evt_el.y)};
                float timestamp{static_cast<float>(evt_el.timestamp - (*evt_data.begin()).timestamp)}; // Relative timestamp
                float polarity{static_cast<float>(evt_el.polarity)};
                evt_vector.push_back(glm::vec4{x, y, timestamp, polarity});
            }

            return evt_vector;
        }

        /**
         * @brief Exposes frame data with absolute timestamp as a vector of pairs containing image data and timestamp. 
         */
        std::vector<std::pair<cv::Mat, float>> get_frame_vector_absolute()
        {
            std::vector<std::pair<cv::Mat, float>> frame_vector{};
            cull_elements(frame_vector, frame_data, 0.8f, 0.5f);
            for (const FrameDatum &frame_el : frame_data)
            {
                float timestamp{static_cast<float>(frame_el.timestamp)};
                frame_vector.push_back(std::make_pair(frame_el.frameData, timestamp));
            }

            return frame_vector;
        }

        /**
         * @brief Exposes frame data with relative timestamp as a vector of pairs containing image data and timestamp.
         */
        std::vector<std::pair<cv::Mat, float>> get_frame_vector_relative()
        {
            std::vector<std::pair<cv::Mat, float>> frame_vector{};
            cull_elements(frame_vector, frame_data, 0.8f, 0.5f);
            for (const FrameDatum &frame_el : frame_data)
            {
                float timestamp{static_cast<float>(frame_el.timestamp - (*frame_data.begin()).timestamp)};
                frame_vector.push_back(std::make_pair(frame_el.frameData, timestamp));
            }

            return frame_vector;
        }

        uint64_t get_earliest_evt_timestamp()
        {
            if (evt_data.empty())
            {
                return -1; // No evt data
            }

            return (*evt_data.begin()).timestamp;
        }

        uint64_t get_earliest_frame_timestamp()
        {
            if (frame_data.empty())
            {
                return -1; // No frame data
            }

            return (*frame_data.begin()).timestamp;
        }

	private:
		std::multiset<EventDatum> evt_data; // Ensures ordered event data

		std::multiset<FrameDatum> frame_data; // Ensures ordered frame data

		/**
		  * @brief Memory management code.
		  */
		template<typename T, typename V>
		void cull_elements(std::vector<T> &vector_data, std::multiset<V> &data, float max_percentage, float cull_percentage)
		{
            size_t maxElements{std::min(vector_data.max_size(), data.max_size())};
            if (data.size() >= static_cast<size_t>(max_percentage * maxElements)) // If there are more than max_percentage max number of events
            {
                // Ensures upper bound is met no matter the condition
                while (data.size() > static_cast<size_t>(cull_percentage * maxElements))
                {
                    data.erase(data.begin(), std::next(data.begin(), static_cast<size_t>((max_percentage - cull_percentage) * maxElements))); // Should bring number of elements down to cull_percentage of max elements
                }
            }
		}	
};

// Operator overloads necessary to use Datum internal structs as keys to multiset
// Define < operator for EventDatum
inline bool operator<(const EventData::EventDatum &left, const EventData::EventDatum &right)
{
    return left.timestamp < right.timestamp;
}

// Define < operator for FrameDatum
inline bool operator<(const EventData::FrameDatum& left, const EventData::FrameDatum& right)
{
    return left.timestamp < right.timestamp;
}

#endif // EVENTDATA_HH
