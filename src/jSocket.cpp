#include "jSocket.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

jSocket::jSocket(uint16_t PORT, PROTO proto)
  : _port(PORT)
  , _proto(proto)
{
	CreateSocket();
}

jSocket::jSocket(int raw_socket_fd, PROTO proto)
  : _socket_fd(raw_socket_fd)
  , _proto(proto)
{
	struct sockaddr sock_address;
	socklen_t len = sizeof(sock_address);
	if (getpeername(raw_socket_fd, &sock_address, &len) == 0)
	{
		address = *(struct sockaddr_in*)&sock_address;
		_port = ntohs(address.sin_port);
		return;
	}
	throw std::runtime_error("[jSocket] - unable to create from raw socket");
}

jSocket::~jSocket()
{
	close(_socket_fd);
}

void jSocket::CreateSocket()
{
	_socket_fd = IsTcp() ? socket(AF_INET, SOCK_STREAM, IPPROTO_IP) : socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (_socket_fd == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
}

bool jSocket::Bind()
{
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(_port);

	if (bind(_socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		return false;
	}
	std::cout << "[CreateSocket] - Socket bound to port " << _port << "\n";
	return true;
}

bool jSocket::IsTcp() const
{
	return _proto == PROTO::TCP;
};

bool jSocket::IsUdp() const
{
	return _proto == PROTO::UDP;
};

bool jSocket::IsSCTP() const
{
	return _proto == PROTO::SCTP;
};

void jSocket::Listen()
{
	if (IsUdp())
	{
		throw std::runtime_error("[jSocket] - Listen not supported on UDP socket");
	}
	if (listen(_socket_fd, SOMAXCONN) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}
	std::cout << "[jSocket] - Listening on * " << _port << "\n";
}

std::unique_ptr<jSocket> jSocket::Accept(std::chrono::milliseconds time_out)
{
	if (IsUdp())
	{
		throw std::runtime_error("[jSocket] - Accept not supported on UDP socket");
	}

	int accepted_socket_fd = accept(_socket_fd, nullptr, nullptr);
	if (accepted_socket_fd < 0)
	{
		perror("accept");
		return nullptr;
	}
	auto accepted_socket = std::make_unique<jSocket>(accepted_socket_fd, _proto);
	return accepted_socket;
}

ReadResult jSocket::Read()
{
	unsigned char read_buffer[1024] = { 0 };
	std::vector<unsigned char> data_buffer;
	auto read_start = std::chrono::steady_clock::now();

	int bytes_read;


	bytes_read = read(_socket_fd, read_buffer, 1024);
	if (bytes_read == 0)
	{
		return ReadError::ConnectionClosed;
	}
	if (bytes_read == -1)
	{
		return ReadError::UnknownError;
	}
	data_buffer.insert(data_buffer.end(), read_buffer, read_buffer + bytes_read);

	return data_buffer;
}

void jSocket::Write(const std::vector<unsigned char>& data_buffer)
{
	auto bytes_written = write(_socket_fd, data_buffer.data(), static_cast<int>(data_buffer.size()));
}

void jSocket::Close()
{
	close(_socket_fd);
}