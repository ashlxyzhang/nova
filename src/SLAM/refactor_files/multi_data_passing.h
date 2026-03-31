/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2009, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

#ifndef MULTI_DATA_PASSING_H_
#define MULTI_DATA_PASSING_H_

// #include "message_filters/synchronizer.h"
// #include "message_filters/connection.h"
// #include "message_filters/null_types.h"
// #include "message_filters/signal9.h"

// #include <boost/tuple/tuple.hpp>
// #include <boost/shared_ptr.hpp>
// #include <boost/function.hpp>
// #include <boost/thread/mutex.hpp>

// #include <boost/bind/bind.hpp>
// #include <boost/type_traits/is_same.hpp>
// #include <boost/noncopyable.hpp>
// #include <boost/mpl/or.hpp>
// #include <boost/mpl/at.hpp>
// #include <boost/mpl/vector.hpp>

// #include <ros/assert.h>
// #include <ros/message_traits.h>
// #include <ros/message_event.h>

#include <deque>
#include <vector>
#include <string>
#include <cassert>
#include <tuple>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>

// namespace message_filters
// {
// namespace sync_policies
// {

struct NullType {};

// namespace mpl = boost::mpl;

template<typename M0, typename M1, typename M2 = NullType, typename M3 = NullType, typename M4 = NullType,
         typename M5 = NullType, typename M6 = NullType, typename M7 = NullType, typename M8 = NullType>
class MultiDataPassing
{

  // typedef std::tuple<M0, M1, M2, M3, M4, M5, M6, M7, M8> Messages;
  // typedef Signal9<M0, M1, M2, M3, M4, M5, M6, M7, M8> Signal;
  typedef std::chrono::time_point<std::chrono::steady_clock> timePoint; 
  typedef 
  std::tuple<
  std::pair<std::shared_ptr<M0>, timePoint>, 
  std::pair<std::shared_ptr<M1>, timePoint>, 
  std::pair<std::shared_ptr<M2>, timePoint>, 
  std::pair<std::shared_ptr<M3>, timePoint>,
  std::pair<std::shared_ptr<M4>, timePoint>, 
  std::pair<std::shared_ptr<M5>, timePoint>, 
  std::pair<std::shared_ptr<M6>, timePoint>, 
  std::pair<std::shared_ptr<M7>, timePoint>,
  std::pair<std::shared_ptr<M8>, timePoint> > Events;
  
  // typedef duration;

  // typedef typename mpl::fold<Messages, mpl::int_<0>, mpl::if_<mpl::not_<boost::is_same<mpl::_2, NullType> >, mpl::next<mpl::_1>, mpl::_1> >::type RealTypeCount;
  typedef typename std::pair<std::shared_ptr<M0>, timePoint> M0Event;
  typedef typename std::pair<std::shared_ptr<M1>, timePoint> M1Event;
  typedef typename std::pair<std::shared_ptr<M2>, timePoint> M2Event;
  typedef typename std::pair<std::shared_ptr<M3>, timePoint> M3Event;
  typedef typename std::pair<std::shared_ptr<M4>, timePoint> M4Event;
  typedef typename std::pair<std::shared_ptr<M5>, timePoint> M5Event;
  typedef typename std::pair<std::shared_ptr<M6>, timePoint> M6Event;
  typedef typename std::pair<std::shared_ptr<M7>, timePoint> M7Event;
  typedef typename std::pair<std::shared_ptr<M8>, timePoint> M8Event;

  // typedef Synchronizer<ApproximateTime> Sync;
  // typedef PolicyBase<M0, M1, M2, M3, M4, M5, M6, M7, M8> Super;
  // typedef typename Super::Messages Messages;
  // typedef typename Super::Signal Signal;
  // typedef typename Super::Events Events;
  // typedef typename Super::RealTypeCount RealTypeCount;
  // typedef typename Super::M0Event M0Event;
  // typedef typename Super::M1Event M1Event;
  // typedef typename Super::M2Event M2Event;
  // typedef typename Super::M3Event M3Event;
  // typedef typename Super::M4Event M4Event;
  // typedef typename Super::M5Event M5Event;
  // typedef typename Super::M6Event M6Event;
  // typedef typename Super::M7Event M7Event;
  // typedef typename Super::M8Event M8Event;
  typedef std::deque<M0Event> M0Deque;
  typedef std::deque<M1Event> M1Deque;
  typedef std::deque<M2Event> M2Deque;
  typedef std::deque<M3Event> M3Deque;
  typedef std::deque<M4Event> M4Deque;
  typedef std::deque<M5Event> M5Deque;
  typedef std::deque<M6Event> M6Deque;
  typedef std::deque<M7Event> M7Deque;
  typedef std::deque<M8Event> M8Deque;
  typedef std::vector<M0Event> M0Vector;
  typedef std::vector<M1Event> M1Vector;
  typedef std::vector<M2Event> M2Vector;
  typedef std::vector<M3Event> M3Vector;
  typedef std::vector<M4Event> M4Vector;
  typedef std::vector<M5Event> M5Vector;
  typedef std::vector<M6Event> M6Vector;
  typedef std::vector<M7Event> M7Vector;
  typedef std::vector<M8Event> M8Vector;
  typedef std::tuple<M0Event, M1Event, M2Event, M3Event, M4Event, M5Event, M6Event, M7Event, M8Event> Tuple;
  typedef std::tuple<M0Deque, M1Deque, M2Deque, M3Deque, M4Deque, M5Deque, M6Deque, M7Deque, M8Deque> DequeTuple;
  typedef std::tuple<M0Vector, M1Vector, M2Vector, M3Vector, M4Vector, M5Vector, M6Vector, M7Vector, M8Vector> VectorTuple;

public:
  MultiDataPassing(uint32_t queue_size, uint32_t output_queue_size, int numTypes, std::condition_variable* cv)
  : //parent_(0), 
    queue_size_(queue_size)
  , output_queue_size(output_queue_size)
  , cv(cv)
  , enable_reset_(false)
  , num_reset_deques_(0)
  , num_non_empty_deques_(0)
  , pivot_(NO_PIVOT)
  , max_interval_duration_(std::chrono::steady_clock::duration::max())
  , age_penalty_(0.1)
  // , has_dropped_messages_(9, false)
  // , inter_message_lower_bounds_(9, )
  // , warned_about_incorrect_bound_(9, false)
  // , last_stamps_(9, timePoint::min())
  , realTypeCount(numTypes)

  {
    assert(queue_size_ > 0);  // The synchronizer will tend to drop many messages with a queue size of 1. At least 2 is recommended.
    inter_message_lower_bounds_.resize(9, std::chrono::steady_clock::duration::zero());
    warned_about_incorrect_bound_.resize(9, false);
    has_dropped_messages_.resize(9, false);
    last_stamps_.resize(9, timePoint::min());
  }

private:
  template<int i>
  bool checkInterMessageBound()
  {
    // namespace mt = ros::message_traits;

    typename std::tuple_element<i, DequeTuple>::type& deque = std::get<i>(deques_);
    typename std::tuple_element<i, VectorTuple>::type& v = std::get<i>(past_);
    // assert(!deque.empty());
    assert(!deque.empty());
    // const typename std::tuple_element<i, Messages>::type &msg = *(deque.back()).getMessage();
    // ros::Time msg_time = mt::TimeStamp<typename std::tuple_element<i, Messages>::type>::value(msg);
    timePoint msg_time = deque.back().second;
    timePoint previous_msg_time;
    bool check_ok = true;
    if (deque.size() == (size_t) 1)
    {
      if (v.empty())
      {
        // We have already published (or have never received) the previous message, we cannot check the bound
        return check_ok;
      }
      // const typename std::tuple_element<i, Messages>::type &previous_msg = *(v.back()).getMessage();
      previous_msg_time = v.back().second;
    }
    else
    {
      // There are at least 2 elements in the deque. Check that the gap respects the bound if it was provided.
      // const typename std::tuple_element<i, Messages>::type &previous_msg = *(deque[deque.size()-2]).getMessage();
      // previous_msg_time =  mt::TimeStamp<typename std::tuple_element<i, Messages>::type>::value(previous_msg);
      previous_msg_time = deque.at(deque.size()-2).second;
    }
    if (msg_time < previous_msg_time)
    {
      if (!warned_about_incorrect_bound_[i])
        std::cerr<<"Messages of type " << i << " arrived out of order (will print only once)";
      warned_about_incorrect_bound_[i] = true;
      check_ok = false;
    }
    else if ((msg_time - previous_msg_time) < inter_message_lower_bounds_[i])
    {
      if (!warned_about_incorrect_bound_[i])
        std::cerr<<"Messages of type " << i << " arrived closer (" << (msg_time - previous_msg_time).count()
                        << ") than the lower bound you provided (" << inter_message_lower_bounds_[i].count()
                        << ") (will print only once)";
      warned_about_incorrect_bound_[i] = true;
      check_ok = false;
    }
    return check_ok;
  }


public:
  template<int i>
  void add(const typename std::tuple_element<i, Events>::type& evt)
  {
    std::scoped_lock lock(data_mutex_);

    // check if time jumped back in simulation time
    timePoint now = evt.second;
    if (enable_reset_)
    {
      if (now < last_stamps_[i])
      {
        ++num_reset_deques_;
        if (num_reset_deques_ == 1)
        {
          std::cerr<<"Detected jump back in time. Clearing message filter queues";
        }
        clearDeque<i>();
        if (num_reset_deques_ >= realTypeCount)
        {
          num_reset_deques_ = 0;
        }
      }
    }
    last_stamps_[i] = now;
    //std::cout<<"here1"<<std::endl;
    std::deque<typename std::tuple_element<i, Events>::type>& deque = std::get<i>(deques_);
    deque.push_back(evt);
    if (deque.size() == (size_t)1) {
      // We have just added the first message, so it was empty before
      ++num_non_empty_deques_;
      if (num_non_empty_deques_ == (uint32_t)realTypeCount)
      {
        //std::cout<<"here2"<<std::endl;
        // All deques have messages
        process();
      }
    }
    else
    {
      //std::cout<<"here3"<<std::endl;
      if (!checkInterMessageBound<i>())
        if (enable_reset_)
        {
          dequeDeleteFront<i>();
        }
    }
    // Check whether we have more messages than allowed in the queue.
    // Note that during the above call to process(), queue i may contain queue_size_+1 messages.
    std::vector<typename std::tuple_element<i, Events>::type>& past = std::get<i>(past_);
    //std::cout<<"here4"<<std::endl;
    if (deque.size() + past.size() > queue_size_)
    {
      //std::cout<<"here5"<<std::endl;
      // Cancel ongoing candidate search, if any:
      num_non_empty_deques_ = 0; // We will recompute it from scratch
      recover<0>();
      recover<1>();
      recover<2>();
      recover<3>();
      recover<4>();
      recover<5>();
      recover<6>();
      recover<7>();
      recover<8>();
      // Drop the oldest message in the offending topic
      assert(!deque.empty());
      deque.pop_front();
      has_dropped_messages_[i] = true;
      //std::cout<<"here6"<<std::endl;
      if (pivot_ != NO_PIVOT)
      {
        //std::cout<<"here7"<<std::endl;
        // The candidate is no longer valid. Destroy it.
        candidate_ = Tuple();
        pivot_ = NO_PIVOT;
        // There might still be enough messages to create a new candidate:
        process();
      }
    }
  }

private:

  void setAgePenalty(double age_penalty)
  {
    // For correctness we only need age_penalty > -1.0, but most likely a negative age_penalty is a mistake.
    assert(age_penalty >= 0);
    age_penalty_ = age_penalty;
  }

  void setInterMessageLowerBound(int i, std::chrono::steady_clock::duration lower_bound) {
    assert(lower_bound >= std::chrono::steady_clock::duration::zero());
    inter_message_lower_bounds_[i] = lower_bound;
  }

  void setInterMessageLowerBound(std::chrono::steady_clock::duration lower_bound) {
    assert(lower_bound >= std::chrono::steady_clock::duration::zero());
    for (size_t i = 0; i < inter_message_lower_bounds_.size(); i++)
    {
      inter_message_lower_bounds_[i] = lower_bound;
    }
  }

  void setMaxIntervalDuration(std::chrono::steady_clock::duration max_interval_duration) {
    assert(max_interval_duration >= std::chrono::steady_clock::duration::zero());
    max_interval_duration_ = max_interval_duration;
  }

  void setReset(const bool reset)
  {
    // Set this true to reset queue on ROS time jumped back
    enable_reset_ = reset;
  }

private:
  template<int i>
  void clearDeque()
  {
    num_non_empty_deques_ = 0;
    recover<0>();
    recover<1>();
    recover<2>();
    recover<3>();
    recover<4>();
    recover<5>();
    recover<6>();
    recover<7>();
    recover<8>();
    std::deque<typename std::tuple_element<i, Events>::type>& q = std::get<i>(deques_);
    if (!q.empty())
    {
      --num_non_empty_deques_;
    }
    q.clear();
    warned_about_incorrect_bound_[i] = false;
    candidate_ = Tuple();
    pivot_ = NO_PIVOT;
  }

  // Assumes that deque number <index> is non empty
  template<int i>
  void dequeDeleteFront()
  {
    std::deque<typename std::tuple_element<i, Events>::type>& deque = std::get<i>(deques_);
    assert(!deque.empty());
    deque.pop_front();
    if (deque.empty())
    {
      --num_non_empty_deques_;
    }
  }

  // Assumes that deque number <index> is non empty
  void dequeDeleteFront(uint32_t index)
  {
    switch (index)
    {
    case 0:
      dequeDeleteFront<0>();
      break;
    case 1:
      dequeDeleteFront<1>();
      break;
    case 2:
      dequeDeleteFront<2>();
      break;
    case 3:
      dequeDeleteFront<3>();
      break;
    case 4:
      dequeDeleteFront<4>();
      break;
    case 5:
      dequeDeleteFront<5>();
      break;
    case 6:
      dequeDeleteFront<6>();
      break;
    case 7:
      dequeDeleteFront<7>();
      break;
    case 8:
      dequeDeleteFront<8>();
      break;
    }
  }

  // Assumes that deque number <index> is non empty
  template<int i>
  void dequeMoveFrontToPast()
  {
    std::deque<typename std::tuple_element<i, Events>::type>& deque = std::get<i>(deques_);
    std::vector<typename std::tuple_element<i, Events>::type>& vector = std::get<i>(past_);
    assert(!deque.empty());
    vector.push_back(deque.front());
    deque.pop_front();
    if (deque.empty())
    {
      --num_non_empty_deques_;
    }
  }
  // Assumes that deque number <index> is non empty
  void dequeMoveFrontToPast(uint32_t index)
  {
    switch (index)
    {
    case 0:
      dequeMoveFrontToPast<0>();
      break;
    case 1:
      dequeMoveFrontToPast<1>();
      break;
    case 2:
      dequeMoveFrontToPast<2>();
      break;
    case 3:
      dequeMoveFrontToPast<3>();
      break;
    case 4:
      dequeMoveFrontToPast<4>();
      break;
    case 5:
      dequeMoveFrontToPast<5>();
      break;
    case 6:
      dequeMoveFrontToPast<6>();
      break;
    case 7:
      dequeMoveFrontToPast<7>();
      break;
    case 8:
      dequeMoveFrontToPast<8>();
      break;
    }
  }

  void makeCandidate()
  {
    //printf("Creating candidate\n");
    // Create candidate tuple
    candidate_ = Tuple(); // Discards old one if any
    std::get<0>(candidate_) = std::get<0>(deques_).front();
    std::get<1>(candidate_) = std::get<1>(deques_).front();
    if (realTypeCount > 2)
    {
      std::get<2>(candidate_) = std::get<2>(deques_).front();
      if (realTypeCount > 3)
      {
	std::get<3>(candidate_) = std::get<3>(deques_).front();
	if (realTypeCount > 4)
	{
	  std::get<4>(candidate_) = std::get<4>(deques_).front();
	  if (realTypeCount > 5)
	  {
	    std::get<5>(candidate_) = std::get<5>(deques_).front();
	    if (realTypeCount > 6)
	    {
	      std::get<6>(candidate_) = std::get<6>(deques_).front();
	      if (realTypeCount > 7)
	      {
		std::get<7>(candidate_) = std::get<7>(deques_).front();
		if (realTypeCount > 8)
		{
		  std::get<8>(candidate_) = std::get<8>(deques_).front();
		}
	      }
	    }
	  }
	}
      }
    }
    // Delete all past messages, since we have found a better candidate
    std::get<0>(past_).clear();
    std::get<1>(past_).clear();
    std::get<2>(past_).clear();
    std::get<3>(past_).clear();
    std::get<4>(past_).clear();
    std::get<5>(past_).clear();
    std::get<6>(past_).clear();
    std::get<7>(past_).clear();
    std::get<8>(past_).clear();
    //printf("Candidate created\n");
  }


  // ASSUMES: num_messages <= past_[i].size()
  template<int i>
  void recover(size_t num_messages)
  {
    if (i >= realTypeCount)
    {
      return;
    }

    std::vector<typename std::tuple_element<i, Events>::type>& v = std::get<i>(past_);
    std::deque<typename std::tuple_element<i, Events>::type>& q = std::get<i>(deques_);
    assert(num_messages <= v.size());
    while (num_messages > 0)
    {
      q.push_front(v.back());
      v.pop_back();
      num_messages--;
    }

    if (!q.empty())
    {
      ++num_non_empty_deques_;
    }
  }


  template<int i>
  void recover()
  {
    if (i >= realTypeCount)
    {
      return;
    }

    std::vector<typename std::tuple_element<i, Events>::type>& v = std::get<i>(past_);
    std::deque<typename std::tuple_element<i, Events>::type>& q = std::get<i>(deques_);
    while (!v.empty())
    {
      q.push_front(v.back());
      v.pop_back();
    }

    if (!q.empty())
    {
      ++num_non_empty_deques_;
    }
  }


  template<int i>
  void recoverAndDelete()
  {
    if (i >= realTypeCount)
    {
      return;
    }

    std::vector<typename std::tuple_element<i, Events>::type>& v = std::get<i>(past_);
    std::deque<typename std::tuple_element<i, Events>::type>& q = std::get<i>(deques_);
    while (!v.empty())
    {
      q.push_front(v.back());
      v.pop_back();
    }

    assert(!q.empty());

    q.pop_front();
    if (!q.empty())
    {
      ++num_non_empty_deques_;
    }
  }

  // Assumes: all deques are non empty, i.e. num_non_empty_deques_ == realTypeCount
  void publishCandidate()
  {
    //std::cout<<"here29, in publihs candidate"<<std::endl;
    //printf("Publishing candidate\n");
    // Publish
    while(outputQueue.size() > output_queue_size)
    {
      outputQueue.pop_front();
      //std::cout<<"here 29.5 are popping"<<std::endl;
    }
    outputQueue.push_back(candidate_);
    //std::cout<<"------------------------------------here 30, just pushed back output queue"<<std::endl;
    cv->notify_one();
    //std::cout<<"here 31, notified"<<std::endl;
    // Delete this candidate
    candidate_ = Tuple();
    pivot_ = NO_PIVOT;

    //std::cout<<"here32, pre recover and delete"<<std::endl;
    // Recover hidden messages, and delete the ones corresponding to the candidate
    num_non_empty_deques_ = 0; // We will recompute it from scratch
    recoverAndDelete<0>();
    recoverAndDelete<1>();
    recoverAndDelete<2>();
    recoverAndDelete<3>();
    recoverAndDelete<4>();
    recoverAndDelete<5>();
    recoverAndDelete<6>();
    recoverAndDelete<7>();
    recoverAndDelete<8>();
    //std::cout<<"here33, post recover and delete"<<std::endl;
  }

  // Assumes: all deques are non empty, i.e. num_non_empty_deques_ == realTypeCount
  // Returns: the oldest message on the deques
  void getCandidateStart(uint32_t &start_index, timePoint &start_time)
  {
    return getCandidateBoundary(start_index, start_time, false);
  }

  // Assumes: all deques are non empty, i.e. num_non_empty_deques_ == realTypeCount
  // Returns: the latest message among the heads of the deques, i.e. the minimum
  //          time to end an interval started at getCandidateStart_index()
  void getCandidateEnd(uint32_t &end_index, timePoint &end_time)
  {
    return getCandidateBoundary(end_index, end_time, true);
  }

  // ASSUMES: all deques are non-empty
  // end = true: look for the latest head of deque
  //       false: look for the earliest head of deque
  void getCandidateBoundary(uint32_t &index, timePoint &time, bool end)
  {
    // namespace mt = ros::message_traits;

    M0Event& m0 = std::get<0>(deques_).front();
    // time = mt::TimeStamp<M0>::value(*m0.getMessage());
    time = m0.second;
    index = 0;
    if (realTypeCount > 1)
    {
      M1Event& m1 = std::get<1>(deques_).front();
      timePoint m1Time = m1.second;
      if ((m1Time < time) ^ end)
      {
        time = m1Time;
        index = 1;
      }
    }
    if (realTypeCount > 2)
    {
      M2Event& m2 = std::get<2>(deques_).front();
      timePoint m2Time = m2.second;
      if ((m2Time < time) ^ end)
      {
        time = m2Time;
        index = 2;
      }
    }
    if (realTypeCount > 3)
    {
      M3Event& m3 = std::get<3>(deques_).front();
      timePoint m3Time = m3.second;
      if ((m3Time < time) ^ end)
      {
        time = m3Time;
        index = 3;
      }
    }
    if (realTypeCount > 4)
    {
      M4Event& m4 = std::get<4>(deques_).front();
      timePoint m4Time = m4.second;
      if ((m4Time < time) ^ end)
      {
        time = m4Time;
        index = 4;
      }
    }
    if (realTypeCount > 5)
    {
      M5Event& m5 = std::get<5>(deques_).front();
      timePoint m5Time = m5.second;
      if ((m5Time < time) ^ end)
      {
        time = m5Time;
        index = 5;
      }
    }
    if (realTypeCount > 6)
    {
      M6Event& m6 = std::get<6>(deques_).front();
      timePoint m6Time = m6.second;
      if ((m6Time < time) ^ end)
      {
        time = m6Time;
        index = 6;
      }
    }
    if (realTypeCount > 7)
    {
      M7Event& m7 = std::get<7>(deques_).front();
      timePoint m7Time = m7.second;
      if ((m7Time < time) ^ end)
      {
        time = m7Time;
        index = 7;
      }
    }
    if (realTypeCount > 8)
    {
      M8Event& m8 = std::get<8>(deques_).front();
      timePoint m8Time = m8.second;
      if ((m8Time < time) ^ end)
      {
        time = m8Time;
        index = 8;
      }
    }
  }


  // ASSUMES: we have a pivot and candidate
  template<int i>
  timePoint getVirtualTime()
  {
    // namespace mt = ros::message_traits;

    if (i >= realTypeCount)
    {
      return timePoint::min();  // Dummy return value
    }
    assert(pivot_ != NO_PIVOT);

    std::vector<typename std::tuple_element<i, Events>::type>& v = std::get<i>(past_);
    std::deque<typename std::tuple_element<i, Events>::type>& q = std::get<i>(deques_);
    if (q.empty())
    {
      assert(!v.empty());  // Because we have a candidate
      // ros::Time last_msg_time = mt::TimeStamp<typename std::tuple_element<i, Messages>::type>::value(*(v.back()).getMessage());
      timePoint last_msg_time = v.back().second;
      timePoint msg_time_lower_bound = last_msg_time + inter_message_lower_bounds_[i];
      if (msg_time_lower_bound > pivot_time_)  // Take the max
      {
        return msg_time_lower_bound;
      }
      return pivot_time_;
    }
    // ros::Time current_msg_time = mt::TimeStamp<typename std::tuple_element<i, Messages>::type>::value(*(q.front()).getMessage());
    timePoint current_msg_time = q.front().second;
    return current_msg_time;
  }


  // ASSUMES: we have a pivot and candidate
  void getVirtualCandidateStart(uint32_t &start_index, timePoint &start_time)
  {
    return getVirtualCandidateBoundary(start_index, start_time, false);
  }

  // ASSUMES: we have a pivot and candidate
  void getVirtualCandidateEnd(uint32_t &end_index, timePoint &end_time)
  {
    return getVirtualCandidateBoundary(end_index, end_time, true);
  }

  // ASSUMES: we have a pivot and candidate
  // end = true: look for the latest head of deque
  //       false: look for the earliest head of deque
  void getVirtualCandidateBoundary(uint32_t &index, timePoint &time, bool end)
  {
    // namespace mt = ros::message_traits;

    std::vector<timePoint> virtual_times(9);
    virtual_times[0] = getVirtualTime<0>();
    virtual_times[1] = getVirtualTime<1>();
    virtual_times[2] = getVirtualTime<2>();
    virtual_times[3] = getVirtualTime<3>();
    virtual_times[4] = getVirtualTime<4>();
    virtual_times[5] = getVirtualTime<5>();
    virtual_times[6] = getVirtualTime<6>();
    virtual_times[7] = getVirtualTime<7>();
    virtual_times[8] = getVirtualTime<8>();
 
    time = virtual_times[0];
    index = 0;
    for (int i = 0; i < realTypeCount; i++)
    {
      if ((virtual_times[i] < time) ^ end)
      {
	time = virtual_times[i];
	index = i;
      }
    }
  }


  // assumes data_mutex_ is already locked
  void process()
  {
    //std::cout<<"here8, in process start"<<std::endl;
    // While no deque is empty
    while (num_non_empty_deques_ == (uint32_t)realTypeCount)
    {
      //std::cout<<"here9, while looping"<<std::endl;
      // Find the start and end of the current interval
      //printf("Entering while loop in this state [\n");
      //show_internal_state();
      //printf("]\n");
      timePoint end_time, start_time;
      uint32_t end_index, start_index;
      getCandidateEnd(end_index, end_time);
      getCandidateStart(start_index, start_time);
      //std::cout<<"here10 got candidates"<<std::endl;
      for (uint32_t i = 0; i < (uint32_t)realTypeCount; i++)
      {
        if (i != end_index)
        {
          // No dropped message could have been better to use than the ones we have,
          // so it becomes ok to use this topic as pivot in the future
          has_dropped_messages_[i] = false;
        }
      }
      //std::cout<<"here11, end of for"<<std::endl;
      if (pivot_ == NO_PIVOT)
      {
        //std::cout<<"here 12no pivot"<<std::endl;
        // We do not have a candidate
        // INVARIANT: the past_ vectors are empty
        // INVARIANT: (candidate_ has no filled members)
        if (end_time - start_time > max_interval_duration_)
        {
          // This interval is too big to be a valid candidate, move to the next
          dequeDeleteFront(start_index);
          continue;
        }
        if (has_dropped_messages_[end_index])
        {
          // The topic that would become pivot has dropped messages, so it is not a good pivot
          dequeDeleteFront(start_index);
          continue;
        }
        //std::cout<<"here13"<<std::endl;
        // This is a valid candidate, and we don't have any, so take it
        makeCandidate();
        candidate_start_ = start_time;
        candidate_end_ = end_time;
        pivot_ = end_index;
        pivot_time_ = end_time;
        dequeMoveFrontToPast(start_index);
        //std::cout<<"here14"<<std::endl;
      }
      else
      {
        //std::cout<<"here15"<<std::endl;
        // We already have a candidate
        // Is this one better than the current candidate?
        // INVARIANT: has_dropped_messages_ is all false
        if ((end_time - candidate_end_) * (1 + age_penalty_) >= (start_time - candidate_start_))
        {
          // This is not a better candidate, move to the next
          //std::cout<<"here16"<<std::endl;
          dequeMoveFrontToPast(start_index);
          //std::cout<<"here17"<<std::endl;
        }
        else
        {
          //std::cout<<"here18"<<std::endl;
          // This is a better candidate
          makeCandidate();
          candidate_start_ = start_time;
          candidate_end_ = end_time;
          dequeMoveFrontToPast(start_index);
          //std::cout<<"here19"<<std::endl;
          // Keep the same pivot (and pivot time)
        }
      }
      // INVARIANT: we have a candidate and pivot
      assert(pivot_ != NO_PIVOT);
      //std::cout<<"here20"<<std::endl;
      //printf("start_index == %d, pivot_ == %d\n", start_index, pivot_);
      if (start_index == pivot_)  // TODO: replace with start_time == pivot_time_
      {
        //std::cout<<"here21, publishing"<<std::endl;
        // We have exhausted all possible candidates for this pivot, we now can output the best one
        publishCandidate();
      }
      else if ((end_time - candidate_end_) * (1 + age_penalty_) >= (pivot_time_ - candidate_start_))
      {
        //std::cout<<"here22, publishing 2"<<std::endl;
        // We have not exhausted all candidates, but this candidate is already provably optimal
        // Indeed, any future candidate must contain the interval [pivot_time_ end_time], which
        // is already too big.
        // Note: this case is subsumed by the next, but it may save some unnecessary work and
        //       it makes things (a little) easier to understand
        publishCandidate();
      }
      else if (num_non_empty_deques_ < (uint32_t)realTypeCount)
      {
        //std::cout<<"here23, oakdwakd"<<std::endl;
        uint32_t num_non_empty_deques_before_virtual_search = num_non_empty_deques_;

        // Before giving up, use the rate bounds, if provided, to further try to prove optimality
        std::vector<int> num_virtual_moves(9,0);
        while (1)
        {
          //std::cout<<"here24"<<std::endl;
          timePoint end_time, start_time;
          uint32_t end_index, start_index;
          getVirtualCandidateEnd(end_index, end_time);
          getVirtualCandidateStart(start_index, start_time);
          //std::cout<<"here25"<<std::endl;
          if ((end_time - candidate_end_) * (1 + age_penalty_) >= (pivot_time_ - candidate_start_))
          {
            //std::cout<<"here26, publishing"<<std::endl;
            // We have proved optimality
            // As above, any future candidate must contain the interval [pivot_time_ end_time], which
            // is already too big.
            publishCandidate();  // This cleans up the virtual moves as a byproduct
            break;  // From the while(1) loop only
          }
          if ((end_time - candidate_end_) * (1 + age_penalty_) < (start_time - candidate_start_))
          {
            //std::cout<<"here27, cna't prove"<<std::endl;
            // We cannot prove optimality
            // Indeed, we have a virtual (i.e. optimistic) candidate that is better than the current
            // candidate
            // Cleanup the virtual search:
            num_non_empty_deques_ = 0; // We will recompute it from scratch
            recover<0>(num_virtual_moves[0]);
            recover<1>(num_virtual_moves[1]);
            recover<2>(num_virtual_moves[2]);
            recover<3>(num_virtual_moves[3]);
            recover<4>(num_virtual_moves[4]);
            recover<5>(num_virtual_moves[5]);
            recover<6>(num_virtual_moves[6]);
            recover<7>(num_virtual_moves[7]);
            recover<8>(num_virtual_moves[8]);
            (void)num_non_empty_deques_before_virtual_search; // unused variable warning stopper
            assert(num_non_empty_deques_before_virtual_search == num_non_empty_deques_);
            break;
          }
          //std::cout<<"here28"<<std::endl;
          // Note: we cannot reach this point with start_index == pivot_ since in that case we would
          //       have start_time == pivot_time, in which case the two tests above are the negation
          //       of each other, so that one must be true. Therefore the while loop always terminates.
          assert(start_index != pivot_);
          assert(start_time < pivot_time_);
          dequeMoveFrontToPast(start_index);
          num_virtual_moves[start_index]++;
        } // while(1)
      }
    } // while(num_non_empty_deques_ == (uint32_t)realTypeCount)
  }


  // --------------NEW FUNCTIONS----------------
public:
  MultiDataPassing(){}

  MultiDataPassing &operator=(const MultiDataPassing &other)
  {
    this->queue_size_ = other.queue_size_;
    this->output_queue_size = other.output_queue_size;
    this->cv = other.cv;
    this->enable_reset_ = false;
    this->num_reset_deques_ = 0;
    this->num_non_empty_deques_ = 0;
    this->pivot_ = NO_PIVOT;
    this->max_interval_duration_ = std::chrono::steady_clock::duration::max();
    this->age_penalty_ = 0.1;
    this->realTypeCount = other.realTypeCount;
    assert(this->queue_size_ > 0);  // The synchronizer will tend to drop many messages with a queue size of 1. At least 2 is recommended.
    inter_message_lower_bounds_.resize(9, std::chrono::steady_clock::duration::zero());
    warned_about_incorrect_bound_.resize(9, false);
    has_dropped_messages_.resize(9, false);
    last_stamps_.resize(9, timePoint::min());
    return *this;
  }

  // Assumes mutex has been locked
  bool queueEmpty()
  {
    return outputQueue.empty();
  }

  // Assumes mutex has been locked
  Tuple getValues()
  {
    Tuple front = outputQueue.front();
    outputQueue.pop_front();
    return front;
  }

  int getTypeCount()
  {
    return realTypeCount;
  }

  void lock()
  {
    data_mutex_.lock();
  }

  void unlock()
  {
    data_mutex_.unlock();
  }
private:

  // Sync* parent_;
  int realTypeCount;
  uint32_t queue_size_;
  uint32_t output_queue_size;
  std::condition_variable* cv;

  bool enable_reset_;
  uint32_t num_reset_deques_;

  static const uint32_t NO_PIVOT = 9;  // Special value for the pivot indicating that no pivot has been selected
  DequeTuple deques_;
  uint32_t num_non_empty_deques_;
  VectorTuple past_;
  Tuple candidate_;  // NULL if there is no candidate, in which case there is no pivot.
  timePoint candidate_start_;
  timePoint candidate_end_;
  timePoint pivot_time_;
  uint32_t pivot_;  // Equal to NO_PIVOT if there is no candidate
  std::mutex data_mutex_;  // Protects all of the above

  std::chrono::steady_clock::duration  max_interval_duration_; // TODO: initialize with a parameter
  double age_penalty_;

  std::vector<bool> has_dropped_messages_;
  std::vector<std::chrono::steady_clock::duration> inter_message_lower_bounds_;
  std::vector<bool> warned_about_incorrect_bound_;
  std::vector<timePoint> last_stamps_;

  std::deque<Tuple> outputQueue; 
};

// } // namespace sync
// } // namespace message_filters

#endif // MESSAGE_FILTERS_SYNC_APPROXIMATE_TIME_H
