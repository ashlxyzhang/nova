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
        EventData()
            : evt_data{}, frame_data{}, evt_data_lock{}, frame_data_lock{}, evt_data_vector_absolute{},
              evt_data_vector_relative{}, frame_data_vector_absolute{}, frame_data_vector_relative{},
              evt_data_vector_lock{}, frame_data_vector_lock{}, evt_data_vector_need_update{false},
              max_element_percentage{0.8f}, cull_element_percentage{0.5f}
            
            //changed_evt_thread_map{}, changed_evt_thread_map_lock{}, changed_frame_thread_map{}, changed_frame_thread_map_lock{}
		{

		}

        /**
         * @brief Locks event data vectors.
         */
        void lock_evt_data_vector()
        {
            evt_data_vector_lock.lock();
        }

        /**
         * @brief Unlocks event data vectors.
         */
        void unlock_evt_data_vector()
        {
            evt_data_vector_lock.unlock();
        }

        /**
         * @brief Locks frame data vectors.
         */
        void lock_frame_data_vectors()
        {
            frame_data_vector_lock.lock();
        }

        /**
         * @brief Unlocks frame data vectors. 
         */
        void unlock_frame_data_vectors()
        {
            frame_data_vector_lock.unlock();
        }

        /**
         * @brief Inserts event data into internel ordered set of event data.
         */
        void write_evt_data(EventDatum raw_evt_data)
        {
            std::unique_lock<std::recursive_mutex> evt_data_ul{evt_data_lock};
            evt_data.insert(raw_evt_data);
            

            if (!evt_data_vector_need_update && 
                raw_evt_data.timestamp >= (*(--evt_data.end())).timestamp && 
                !cull_elements(evt_data_vector_relative, evt_data, max_element_percentage, cull_element_percentage) && 
                !cull_elements(evt_data_vector_absolute, evt_data, max_element_percentage, cull_element_percentage))
            {
                std::unique_lock<std::recursive_mutex> evt_data_vector_ul{evt_data_vector_lock};

                float x{static_cast<float>(raw_evt_data.x)};
                float y{static_cast<float>(raw_evt_data.y)};
                float timestamp_absolute{static_cast<float>(raw_evt_data.timestamp)};
                float timestamp_relative{static_cast<float>(raw_evt_data.timestamp - get_earliest_evt_timestamp())};
                float polarity{static_cast<float>(raw_evt_data.polarity)};
                evt_data_vector_absolute.push_back(glm::vec4{x, y, timestamp_absolute, polarity});
                evt_data_vector_relative.push_back(glm::vec4{x, y, timestamp_relative, polarity});

                evt_data_vector_ul.unlock();
                
            }
            else
            {
                evt_data_vector_need_update = true;
            }
            evt_data_ul.unlock();
            /*std::unique_lock<std::mutex> changed_evt_map_ul{changed_evt_thread_map_lock};
            changed_evt_thread_map.clear();
            changed_evt_map_ul.unlock();*/
        }

        /**
         * @brief Inserts frame data into internal ordered set of frame data.
         */
        void write_frame_data(FrameDatum raw_frame_data)
        {
            std::unique_lock<std::recursive_mutex> frame_data_ul{frame_data_lock};
            frame_data.insert(raw_frame_data);

            if (!frame_data_vector_need_update &&
                raw_frame_data.timestamp >= (*(--frame_data.end())).timestamp &&
                !cull_elements(frame_data_vector_relative, frame_data, max_element_percentage, cull_element_percentage) &&
                !cull_elements(frame_data_vector_absolute, frame_data, max_element_percentage, cull_element_percentage))
            {
                std::unique_lock<std::recursive_mutex> frame_data_vector_ul{frame_data_vector_lock};
                float timestamp_absolute{static_cast<float>(raw_frame_data.timestamp)};
                float timestamp_relative{static_cast<float>(raw_frame_data.timestamp - get_earliest_frame_timestamp())};
                frame_data_vector_absolute.push_back(std::make_pair(raw_frame_data.frameData, timestamp_absolute));
                frame_data_vector_relative.push_back(std::make_pair(raw_frame_data.frameData, timestamp_relative));
                frame_data_vector_ul.unlock();
            }
            else
            {
                frame_data_vector_need_update = true;
            }

            frame_data_ul.unlock();

            /*std::unique_lock<std::mutex> changed_frame_map_ul{changed_frame_thread_map_lock};
            changed_frame_thread_map.clear();
            changed_frame_map_ul.unlock();*/

        }

        /**
         * @brief Exposes event data with absolute or relative timestamp as a vector of glm::vec4.
         *        Caller must have called lock_evt_data_vectors().
         */
        const std::vector<glm::vec4>& get_evt_vector_ref(bool relative)
        {
            // Update changed_thread_map to indicate this thread has up-to-date data
            /*std::unique_lock<std::mutex> changed_evt_map_ul{changed_evt_thread_map_lock};
            changed_evt_thread_map[tid] = false;
            changed_evt_map_ul.unlock();*/
            // Expensive call
            if (evt_data_vector_need_update)
            {
                update_evt_data_vectors();
            }
            if (relative)
            {
                return evt_data_vector_relative;
            }
            return evt_data_vector_absolute;
        }

        /**
         * @brief Exposes frame data with absolute or relative timestamp as a vector of pairs containing image data and timestamp. 
         *        Caller must have called lock_frame_data_vectors().
         */
        const std::vector<std::pair<cv::Mat, float>>& get_frame_vectors(bool relative)
        {
            // Update changed_thread_map to indicate this thread has up-to-date data
            /*std::unique_lock<std::mutex> changed_frame_map_ul{changed_frame_thread_map_lock};
            changed_frame_thread_map[tid] = false;
            changed_frame_map_ul.unlock();*/
            if (frame_data_vector_need_update)
            {
                update_frame_data_vectors();
            }
            if (relative)
            {
                return frame_data_vector_relative;
            }
            return frame_data_vector_absolute;
        }

        /**
         * @brief For a given thread, determine if event data changed since last call to get_*_vectors
         */
        /*bool is_evt_data_changed(std::thread::id tid)
        {
            std::unique_lock<std::mutex> changed_evt_map_ul{changed_evt_thread_map_lock};
            if (!changed_evt_thread_map.contains(tid))
            {
                changed_evt_map_ul.unlock();
                return true;
            }

            changed_evt_map_ul.unlock();
            return changed_evt_thread_map.at(tid);
        }*/

        /**
         * @brief For a given thread, determine if frame data changed since last call to get_*_vectors
         */
        /*bool is_frame_data_changed(std::thread::id tid)
        {
            std::unique_lock<std::mutex> changed_frame_map_ul{changed_frame_thread_map_lock};
            if (!changed_frame_thread_map.contains(tid))
            {
                changed_frame_map_ul.unlock();
                return true;
            }

            changed_frame_map_ul.unlock();
            return changed_frame_thread_map.at(tid);
        }*/

        /**
         * @brief Gets the earliest event timestamp.
         */
        int64_t get_earliest_evt_timestamp()
        {
            std::unique_lock<std::recursive_mutex> evt_data_ul{evt_data_lock};

            if (evt_data.empty())
            {
                evt_data_ul.unlock();
                return -1; // No evt data
            }

            int64_t earliest_timestamp{(*evt_data.begin()).timestamp};

            evt_data_ul.unlock();

            return earliest_timestamp;
        }

        /**
         * @brief Gets the earliest frame timestamp.
         */
        int64_t get_earliest_frame_timestamp()
        {
            std::unique_lock<std::recursive_mutex> frame_data_ul{frame_data_lock};

            if (frame_data.empty())
            {
                frame_data_ul.unlock();
                return -1; // No frame data
            }

            int64_t timestamp{(*frame_data.begin()).timestamp};

            frame_data_ul.unlock();
            
            return timestamp;
        }

	private:
		std::multiset<EventDatum> evt_data; // Ensures ordered event data

		std::multiset<FrameDatum> frame_data; // Ensures ordered frame data

        std::recursive_mutex evt_data_lock;
        std::recursive_mutex frame_data_lock;

        std::vector<glm::vec4> evt_data_vector_absolute;
        std::vector<glm::vec4> evt_data_vector_relative;
        std::vector<std::pair<cv::Mat, float>> frame_data_vector_absolute;
        std::vector<std::pair<cv::Mat, float>> frame_data_vector_relative;

        std::recursive_mutex evt_data_vector_lock;
        std::recursive_mutex frame_data_vector_lock;

        bool evt_data_vector_need_update; // Flag to indicate if update is needed when vector of event data are exposed.
        bool frame_data_vector_need_update; // Flag to indicate if update is needed when vector of frame data are exposed.

        float max_element_percentage; // What percentage of max size of vector should number of elements populate
        float cull_element_percentage; // What percentage of max size of vector should elements be culled to if they exceed max_element_percentage

        // Hashmaps contains thread id mapped to boolean that indicates if new data was read
        // since the thread called the get_*_vectors functions. This was added because
        // calling these functions is expensive and should only be done
        // if the data was updated.
        /*std::unordered_map<std::thread::id, bool> changed_evt_thread_map;
        std::mutex changed_evt_thread_map_lock;

        std::unordered_map<std::thread::id, bool> changed_frame_thread_map;
        std::mutex changed_frame_thread_map_lock;*/

        /**
         * @brief Updates event data internal vectors.
         * 
         */
        void update_evt_data_vectors()
        {
            std::unique_lock<std::recursive_mutex> evt_data_ul{evt_data_lock};
            std::unique_lock<std::recursive_mutex> evt_data_vector_ul{evt_data_vector_lock};
            evt_data_vector_absolute.clear();
            evt_data_vector_relative.clear();
            cull_elements(evt_data_vector_absolute, evt_data, max_element_percentage, cull_element_percentage);
            cull_elements(evt_data_vector_relative, evt_data, max_element_percentage, cull_element_percentage);
            for (const EventDatum &evt_el : evt_data)
            {
                float x{static_cast<float>(evt_el.x)};
                float y{static_cast<float>(evt_el.y)};
                float timestamp_absolute{static_cast<float>(evt_el.timestamp)};
                float timestamp_relative{static_cast<float>(evt_el.timestamp - get_earliest_evt_timestamp())};
                float polarity{static_cast<float>(evt_el.polarity)};
                evt_data_vector_absolute.push_back(glm::vec4{x, y, timestamp_absolute, polarity});
                evt_data_vector_relative.push_back(glm::vec4{x, y, timestamp_relative, polarity});
            }

            evt_data_vector_need_update = false;

            evt_data_vector_ul.unlock();
            evt_data_ul.unlock();
        }

        /**
         * @brief Updates frame internal vectors.
         */
        void update_frame_data_vectors()
        {
            std::unique_lock<std::recursive_mutex> frame_data_ul{frame_data_lock};
            std::unique_lock<std::recursive_mutex> frame_data_vector_ul{frame_data_vector_lock};
            frame_data_vector_absolute.clear();
            frame_data_vector_relative.clear();
            cull_elements(frame_data_vector_absolute, frame_data, max_element_percentage, cull_element_percentage);
            cull_elements(frame_data_vector_relative, frame_data, 0.8f, 0.5f);
            for (const FrameDatum &frame_el : frame_data)
            {
                
            }

            frame_data_vector_need_update = false;

            frame_data_vector_ul.unlock();
            frame_data_ul.unlock();
        }

		/**
		  * @brief Memory management code. Must be called with appropriate locks.
		  */
		template<typename T, typename V>
		bool cull_elements(std::vector<T> &vector_data, std::multiset<V> &data, float max_percentage, float cull_percentage)
		{
            bool culled = false;
            size_t maxElements{std::min(vector_data.max_size(), data.max_size())};
            if (data.size() >= static_cast<size_t>(max_percentage * maxElements)) // If there are more than max_percentage max number of events
            {
                // Ensures upper bound is met no matter the condition
                while (data.size() > static_cast<size_t>(cull_percentage * maxElements))
                {
                    data.erase(data.begin(), std::next(data.begin(), static_cast<size_t>((max_percentage - cull_percentage) * maxElements))); // Should bring number of elements down to cull_percentage of max elements
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
inline bool operator<(const EventData::FrameDatum& left, const EventData::FrameDatum& right)
{
    return left.timestamp < right.timestamp;
}

#endif // EVENTDATA_HH
