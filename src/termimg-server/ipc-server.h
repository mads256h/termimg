//
// Created by mads on 01/03/2022.
//

#ifndef TERMIMG_IPC_SERVER_H
#define TERMIMG_IPC_SERVER_H

#include <string>
#include <functional>

#include "epoll.h"


class IPCServer {
private:
    int m_fd_socket;
    const std::string m_path;

    Epoll &m_epoll;

    std::function<void(const std::string_view)> m_message_handler;


public:
    explicit IPCServer(std::string path, Epoll &epoll);
    IPCServer(const IPCServer&) = delete;
    ~IPCServer();

    void register_on_message_handler(std::function<void(const std::string_view)> message_handler);

};

#endif //TERMIMG_IPC_SERVER_H
