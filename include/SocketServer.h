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

#define  STDOUT_FD 1

class SocketServer{
    private:
        int server_fd;
        uint16_t _port;
        struct sockaddr_in address;

    public:
        SocketServer(uint16_t PORT);
        ~SocketServer();
        void CreateSocket(void);
        void Listen(std::function<void(std::string , std::promise<std::string> )> callback);
        void Write(const char* Message);
};
#endif