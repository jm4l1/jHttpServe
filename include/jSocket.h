#ifndef _J_SOCKET_H_
#define _J_SOCKET_H_

#include "MessageQueue.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define STDOUT_FD 1
#define MAX_UDP_BUFF_LEN 1024

typedef struct sockaddr_in sockaddr_in_t;
enum class PROTO
{
	TCP,
	UDP,
	SCTP
};

class SocketConnection
{
public:
	SocketConnection() = default;
	SocketConnection(int _raw_socket, PROTO proto)
	  : _connected_socket(_raw_socket)
	  , _proto(proto)
	  , _socket_open(true)
	{
		struct sockaddr address;
		socklen_t len = sizeof(address);
		if (getpeername(_connected_socket, &address, &len) == 0)
		{
			_peer_address = (struct sockaddr_in*)&address;
			port = ntohs(_peer_address->sin_port);
		}
		else
		{
			Close();
		}
	};
	SocketConnection& operator=(const SocketConnection& B)
	{
		_connected_socket = B._connected_socket;
		_proto = B._proto;
		port = B.port;
		_peer_address = B._peer_address;
		return *this;
	}
	~SocketConnection()
	{
		close(_connected_socket);
		std::cout << "closing connection on port :  " << port << "\n";
	};
	void Read(int timeout_usec = 0)
	{
		unsigned char read_buffer[1024] = { 0 };
		std::vector<unsigned char> data_buffer;
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = timeout_usec;
		fd_set read_set;
		FD_ZERO(&read_set);
		FD_SET(_connected_socket, &read_set);

		while (1)
		{
			auto bytes_read = read(_connected_socket, read_buffer, 1024);
			if (bytes_read == 0)
			{
				std::cout << "connection closed by peer\n";
				Close();
				_received_data_buffer.Send({ '\0' });
				return;
			}
			if (bytes_read == -1)
			{
				std::cout << "Error reading on socket\n";
				Close();
				_received_data_buffer.Send({ '\0' });
				return;
			}
			data_buffer.insert(data_buffer.end(), read_buffer, read_buffer + bytes_read);
			memset(read_buffer, 0, 1024);
			_received_data_buffer.Send(std::move(data_buffer));
		}
	}
	void Write(std::vector<unsigned char> message_buffer)
	{
		write(_connected_socket, message_buffer.data(), message_buffer.size());
		return;
	}
	std::vector<unsigned char> GetBuffer()
	{
		if (_socket_open)
		{
			return _received_data_buffer.receive();
		}
		return {};
	};
	void Close()
	{
		close(_connected_socket);
		_socket_open = false;
	};
	bool _socket_open;

private:
	int _connected_socket;
	PROTO _proto;
	uint16_t port;
	struct sockaddr_in* _peer_address;
	MessageQueue<std::vector<unsigned char>> _received_data_buffer;
};

class jSocket
{
public:
	jSocket() = default;
	jSocket(uint16_t PORT, PROTO proto);
	explicit jSocket(int raw_socket_fd, PROTO proto);
	~jSocket();
	void SetPort(uint16_t PORT, PROTO proto)
	{
		_port = PORT;
		_proto = proto;
	};
	void CreateSocket(void);
	void Listen();
	jSocket&& Accept(std::chrono::milliseconds timeout = std::chrono::milliseconds(500));
	void Write(const char* Message);
	int Read(std::vector<unsigned char> data_buffer, std::chrono::milliseconds read_timeout = std::chrono::milliseconds(500));
	bool Bind();
	bool IsTcp() const;
	bool IsSCTP() const;
	bool IsUdp() const;

private:
	int server_fd;
	uint16_t _port;
	PROTO _proto;
	struct sockaddr_in address;
	std::vector<std::future<void>> _connection_threads;
};
#endif