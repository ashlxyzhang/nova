#ifndef DATA_PASSING_H_
#define DATA_PASSING_H_

#include <deque>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <iostream>

template<typename T>
class DataPassingDeque {
    using timePoint = std::chrono::time_point<std::chrono::steady_clock>;
    public:
        DataPassingDeque(){}
        DataPassingDeque(int max_queue_size, std::condition_variable* cv)
        {
            this->max_queue_size = max_queue_size;
            this->cv = cv;
        }

        DataPassingDeque &operator=(const DataPassingDeque &other)
        {
            if(this == &other)
                return *this;
            this->dq.clear();
            // this->data_mutex = other.data_mutex;
            this->max_queue_size = other.max_queue_size;
            this->cv = other.cv;
            return *this;
        }

        // Adds a thing to the queue
        void add(std::shared_ptr<T> thingToAdd, timePoint timestamp)
        {
            lock();
            while(dq.size() >= max_queue_size)
                dq.pop_front();
            dq.push_back({thingToAdd, timestamp});
            unlock();
            cv->notify_one();
        }
        
        void lock()
        {
            data_mutex.lock();
        }

        void unlock()
        {
            data_mutex.unlock();
        }
        
        // Has anything been added to the queue? Assumes mutex is locked
        bool queueEmpty()
        {
            return dq.empty();
        }

        // Gets the first value in the queue. Assumes mutex is locked
        std::pair<std::shared_ptr<T>, timePoint> getValue()
        {
            std::pair<std::shared_ptr<T>, timePoint> value = dq.front();
            dq.pop_front();
            return value;
        }

    private:
        std::mutex data_mutex;
        std::deque<std::pair<std::shared_ptr<T>, timePoint>> dq;
        int max_queue_size;
        std::condition_variable* cv;
};

#endif