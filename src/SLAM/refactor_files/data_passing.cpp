#include <deque>
#include <mutex>
#include <condition_variable>
#include "data_passing.h"

template<typename T>
DataPassingDeque<T>::DataPassingDeque(int max_queue_size, std::condition_variable& cv)
{
    this->max_queue_size = max_queue_size;
    this->cv = cv;
}

template<typename T>
void DataPassingDeque<T>::add(std::shared_ptr<T> thingToAdd, timePoint timestamp)
{
    lock();
    while(dq.size() >= max_queue_size)
        dq.pop_front();
    dq.push_back({thingToPublish, time_stamp});
    changed = true;
    unlock();
    cv.notify_one();
     std::chrono::steady_clock::duration test = std::chrono::steady_clock::duration::max();
}

template<typename T>
bool  DataPassingDeque<T>::hasChanged()
{
    return hasChanged;
}

template<typename T>
std::pair<std::shared_ptr<T>, DataPassingDeque<T>::timePoint> DataPassingDeque<T>::getValue()
{
    std::pair<std::shared_ptr<T>, timePoint> value = dq.front();
    dq.pop_front();
    changed = false;
    return value;
}


template<typename T>
void DataPassingDeque<T>::lock()
{
    data_mutex.lock();
}

template<typename T>
void DataPassingDeque<T>::unlock()
{
    data_mutex.unlock();
}