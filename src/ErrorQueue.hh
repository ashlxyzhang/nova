#pragma once

#ifndef ERROR_QUEUE_HH
#define ERROR_QUEUE_HH

#include <mutex>
#include <queue>
#include <string>
#include <optional>

/**
 * @brief Modules can report error messages here to be logged and reported to GUI for pop up window
 */
class ErrorQueue
{
	private:
		mutable std::mutex mutex;
		std::queue<std::string> error_message_queue;

	public:

		ErrorQueue() = default;

		/**
		 * @brief Adds message to the queue, is thread-safe
		 */
		void push_error(const std::string& error)
		{
			std::lock_guard lock(mutex);
			error_message_queue.push(error);
		}

		/**
		 * @brief Gets oldest error message, returns empty string if there is none
		 */
		std::string pop_error()
		{
			std::lock_guard lock(mutex);

			if (error_message_queue.empty()) return "";
			std::string error = std::move(error_message_queue.front());
			error_message_queue.pop();
			return error;
		}

		/**
		 * @brief thread safe .empty() on the queue
		 */
		bool empty() const
		{
			std::lock_guard lock(mutex);
			return error_message_queue.empty();
		}

		/**
		 * @brief thread safe .size() of the queue
		 */
		size_t size() const
		{
			std::lock_guard lock(mutex);
			return error_message_queue.size();
		}

		/**
		 * @brief thread safe .clear() of the queue
		 */
		void clear()
		{
			std::lock_guard lock(mutex);
			error_message_queue = std::queue<std::string>();
		}
	};

	#endif