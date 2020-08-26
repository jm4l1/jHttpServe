#include "SocketServer.h"
#include <sstream>
#include <iostream>
#include <thread>
SocketServer::SocketServer(uint16_t PORT):_port(PORT){
    CreateSocket();
}
SocketServer::~SocketServer(){
    std::cout << "Terminating Server.\n";
    close(server_fd);
}
void SocketServer::CreateSocket(){
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
void SocketServer::Listen(std::function<void(std::string , std::promise<std::string>&& )> connection_callback){
    int connect_socket;
    int addrlen = sizeof(address);
    struct sockaddr address;
    
    std::cout << "[SocketServer] - Listening on * " << _port << "\n";

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    while(1){  
        if ((connect_socket = accept(server_fd, (struct sockaddr *)&address, 
                                 (socklen_t*)&addrlen))<0){
            perror("accept");
            exit(EXIT_FAILURE);
        }
        //with threading
        //handle on child thread

        auto thread_handler = [&](int &&connect_socket){
            socklen_t len = sizeof(address);
            uint16_t port;
            struct sockaddr_in* addressInternet;
            int valread;
            char buffer[1024] = {0};
            std::promise<std::string> response_prms;
            std::future<std::string> ftr = response_prms.get_future();
             if(getpeername(connect_socket, &address,&len) == 0){
                addressInternet = (struct sockaddr_in*) &address;
                port = ntohs ( addressInternet->sin_port );    
                // std::cout << "Connection received from on child thread " << inet_ntoa( addressInternet->sin_addr) << " on port " << port << "\n";
            }
            valread = read( connect_socket , buffer, 1024);
            connection_callback(std::string(buffer),std::move(response_prms));
            auto response = ftr.get();
            write(connect_socket,response.c_str(),response.size());
            shutdown(connect_socket,SHUT_WR);
            close(connect_socket);
        };
        std::thread t(thread_handler,std::move(connect_socket));
        t.detach();
        // if(getpeername(connect_socket, &address,&len) == 0){
        //     addressInternet = (struct sockaddr_in*) &address;
        //     port = ntohs ( addressInternet->sin_port );    
        //     // std::cout<< "Process created with pid " << pid << "\n";
        //     std::cout << "Connection received from on main thread " << inet_ntoa( addressInternet->sin_addr) << " on port " << port << "\n";
        // }
        // close(connect_socket);
    //     //with forking
    //     pid = fork();
    //     if(pid == 0){
    //        close(server_fd);
    //        server_fd = -1;
    //         if(getpeername(new_socket, &address,&len) == 0){
    //             addressInternet = (struct sockaddr_in*) &address;
    //             port = ntohs ( addressInternet->sin_port );    
    //             std::cout << "Connection received from " << inet_ntoa( addressInternet->sin_addr) << " on port " << port << "\n";
    //         }
    //         valread = read( new_socket , buffer, 1024);
    //         callback(std::string(buffer),new_socket);
    //         shutdown(new_socket,SHUT_WR);
    //         close(new_socket);
    //         new_socket = -1;
    //         exit(EXIT_SUCCESS);
    //    }
    //     else{
    //         if(getpeername(new_socket, &address,&len) == 0){
    //             addressInternet = (struct sockaddr_in*) &address;
    //             port = ntohs ( addressInternet->sin_port );    
    //             std::cout<< "\nProcess created with pid " << pid << "\n";
    //             std::cout << "Connection received from " << inet_ntoa( addressInternet->sin_addr) << " on port " << port << "\n";
    //         }
    //     }
    }    
}
void SocketServer::Write(const char * Message){;
        write(server_fd,Message,strlen(Message));
}