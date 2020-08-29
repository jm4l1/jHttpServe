#ifndef _SOCKET_SERVER_H_
#define _SOCKET_SERVER_H_

#include <cstdint>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <future>
#include <vector>

#define  STDOUT_FD 1

class SocketServer{
    public:
        SocketServer() = default;
        SocketServer(uint16_t PORT);
        ~SocketServer();
        void SetPort(uint16_t PORT){ _port = PORT ;};
        void CreateSocket(void);
        void Listen(std::function<void(std::vector<unsigned char> , std::promise<std::vector<unsigned char>>&&)> callback);
        void Write(const char* Message);
    private:
        int server_fd;
        uint16_t _port;
        struct sockaddr_in address;
};
#endif