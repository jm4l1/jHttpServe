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
  : _proto(proto)
{
	struct sockaddr sock_address;
	socklen_t len = sizeof(sock_address);
	if (getpeername(raw_socket_fd, &sock_address, &len) == 0)
	{
		address = *(struct sockaddr_in*)&sock_address;
		_port = ntohs(address.sin_port);
	}
	throw std::runtime_error("[jSocket] - unable to create from raw socket");
}

jSocket::~jSocket()
{
	std::cout << "Terminating Server.\n";
	close(server_fd);
}

void jSocket::CreateSocket()
{
	std::cout << "[CreateSocket] - Starting jSocket \n";
	server_fd = IsTcp() ? socket(AF_INET, SOCK_STREAM, IPPROTO_IP) : socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (server_fd == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
	std::cout << "[CreateSocket] - Socket Created Successfully with FD:  " << server_fd << "\n";
}

bool jSocket::Bind()
{
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(_port);

	if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
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
	if (listen(server_fd, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}
	std::cout << "[jSocket] - Listening on * " << _port << "\n";
}

jSocket&& jSocket::Accept(std::chrono::milliseconds time_out)
{
	if (IsUdp())
	{
		throw std::runtime_error("[jSocket] - Accept not supported on UDP socket");
	}

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = time_out.count();
	fd_set read_set;
	FD_ZERO(&read_set);
	FD_SET(server_fd, &read_set);

	if (select(server_fd + 1, &read_set, nullptr, nullptr, &timeout))
	{
		perror("select");
		exit(EXIT_FAILURE);
	}

	int accepted_socket_fd = accept(server_fd, nullptr, nullptr);
	if (accepted_socket_fd < 0)
	{
		perror("accept");
		exit(EXIT_FAILURE);
	}
	jSocket accepted_socket = jSocket(accepted_socket_fd, _proto);
	return std::move(accepted_socket);
}

int jSocket::Read(std::vector<unsigned char> data_buffer, std::chrono::milliseconds read_timeout)
{
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = read_timeout.count();
	fd_set read_set;
	FD_ZERO(&read_set);
	FD_SET(server_fd, &read_set);

	auto bytes_read = read(server_fd, data_buffer.data(), 1024);
	if (bytes_read == 0)
	{
		std::cout << "connection closed by peer\n";
	}
	if (bytes_read == -1)
	{
		std::cout << "Error reading on socket\n";
	}
	return bytes_read;
}

void jSocket::Write(const char* Message)
{
	write(server_fd, Message, strlen(Message));
}