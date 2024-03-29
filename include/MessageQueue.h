#ifndef _MESSAGE_QUEUE_H_
#define _MESSAGE_QUEUE_H_

#include <chrono>
#include <deque>
#include <mutex>
#include <optional>

template <typename T>
class MessageQueue
{
public:
	MessageQueue(){};
	T receive()
	{
		std::unique_lock<std::mutex> uLock(_mutex);
		_message_receive_con.wait(uLock,
								  [this]
								  {
									  return !_messages.empty();
								  });
		auto message = std::move(_messages.front());
		_messages.pop_front();

		return message;
	};
	std::optional<T> TryReceive(std::chrono::milliseconds time_out)
	{
		std::unique_lock<std::mutex> uLock(_mutex);
		_message_receive_con.wait_for(uLock,
									  time_out,
									  [this]
									  {
										  return !_messages.empty();
									  });
		if (_messages.empty())
		{
			return std::nullopt;
		}
		auto message = std::move(_messages.front());
		_messages.pop_front();

		return message;
	};
	void Send(T&& message)
	{
		std::lock_guard<std::mutex> Lock(_mutex);
		_messages.push_back(std::move(message));
		_message_receive_con.notify_one();
	};

private:
	std::mutex _mutex;
	std::condition_variable _message_receive_con;
	std::deque<T> _messages;
};
#endif