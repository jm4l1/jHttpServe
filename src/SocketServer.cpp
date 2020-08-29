#include "SocketServer.h"
#include <sstream>
#include <iostream>
#include <thread>
#include <cstring>

SocketServer::SocketServer(uint16_t PORT):_port(PORT)
{
    CreateSocket();
}
SocketServer::~SocketServer()
{
    std::cout << "Terminating Server.\n";
    close(server_fd);
}
void SocketServer::CreateSocket()
{
    std::cout << "[CreateSocket] - Starting SocketServer \n";

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "[CreateSocket] - Socket Created Successfully with FD:  " << server_fd << "\n"; 
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(_port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    std::cout << "[CreateSocket] - Socket bound to port " << _port << "\n";
}
void SocketServer::Listen(std::function<void(std::vector<unsigned char> , std::promise<std::vector<unsigned char> >&& )> connection_callback){
    int connect_socket;
    int addrlen = sizeof(address);
    struct sockaddr address;
    
    std::cout << "[SocketServer] - Listening on * " << _port << "\n";

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    while(1)
    {
        if ((connect_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        auto thread_handler = [&](int &&connect_socket)
        {
            socklen_t len = sizeof(address);
            uint16_t port;
            struct sockaddr_in* addressInternet;
            int valread;
            unsigned char read_buffer[1024] = {0};
            std::vector<unsigned char> data_buffer;
            std::promise<std::vector<unsigned char> > response_prms;
            std::future<std::vector<unsigned char> > ftr = response_prms.get_future();
            
            if(getpeername(connect_socket, &address,&len) == 0)
            {
                addressInternet = (struct sockaddr_in*) &address;
                port = ntohs ( addressInternet->sin_port );
            }
            
            //a time out to allow multiple calls to read system cal.
            //select checks the socket is readeable for this timeout value
            //if it become readable with in the time out the read method is called
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 5000;
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(connect_socket , &read_set);
            
            //read value on connected socket and append to data_buffer
            valread = recv( connect_socket , read_buffer, 1024 ,0 ) ;
            data_buffer.insert(data_buffer.end(),read_buffer , read_buffer + valread);
            memset(read_buffer, 0 , 1024 );

            //wait timeout to see if socket is readable and read again if it is
            select(connect_socket + 1 , &read_set, nullptr , nullptr ,&timeout);
            while(FD_ISSET(connect_socket , &read_set))
            {
                valread = read( connect_socket , read_buffer, 1024 ) ;
                data_buffer.insert(data_buffer.end(),read_buffer , read_buffer + valread);
                memset(read_buffer, 0 , 1024 );
                select(connect_socket + 1 , &read_set, nullptr , nullptr ,&timeout);
            }

            connection_callback(data_buffer,std::move(response_prms));
            auto response = ftr.get();
            write(connect_socket,response.data(),response.size());
            shutdown(connect_socket,SHUT_WR);
            close(connect_socket);
        };
        auto t = std::async(std::launch::async,thread_handler,std::move(connect_socket));
        t.get();
    }    
}
void SocketServer::Write(const char * Message){;
        write(server_fd,Message,strlen(Message));
}