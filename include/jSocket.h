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
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
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
enum class ReadError
{
	ConnectionClosed,
	TimeOut,
	UnknownError
};

using ReadResult = std::variant<std::vector<unsigned char>, ReadError>;
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
	std::unique_ptr<jSocket> Accept(std::chrono::milliseconds timeout = std::chrono::milliseconds(500));
	void Write(const std::vector<unsigned char>& data_buffer);
	ReadResult Read(std::chrono::milliseconds read_timeout = std::chrono::milliseconds(500));
	bool Bind();
	bool IsTcp() const;
	bool IsSCTP() const;
	bool IsUdp() const;

private:
	int _socket_fd;
	uint16_t _port;
	PROTO _proto;
	struct sockaddr_in address;
};
#endif