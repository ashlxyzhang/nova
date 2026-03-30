#ifndef DATA_PASSING_H_
#define DATA_PASSING_H_

#include <deque>
#include <mutex>
#include <chrono>
#include <condition_variable>
// #include <shared_ptr>

// struct 


template<typename T>
class DataPassingDeque {
    using timePoint = std::chrono::time_point<std::chrono::steady_clock>;
    public:
        DataPassingDeque(int max_queue_size, std::condition_variable& cv);
        void add(std::shared_ptr<T> thingToAdd, timePoint timestamp);
        void lock();
        void unlock();
        // Assumes mutex is locked
        bool hasChanged();
        // Assumes mutex is locked
        std::pair<std::shared_ptr<T>, timePoint> getValue();

        private:
        std::mutex data_mutex;
        std::deque<std::pair<std::shared_ptr<T>, timePoint>> dq;
        int max_queue_size;
        std::condition_variable& cv;
        bool changed;
};

#endif