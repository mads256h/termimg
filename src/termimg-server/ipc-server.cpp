//
// Created by mads on 01/03/2022.
//

#include "ipc-server.h"

#include <string>
#include <string_view>
#include <iostream>

#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "epoll.h"

IPCServer::IPCServer(std::string path, Epoll &epoll) : m_path(std::move(path)) {
    std::cout << m_path << std::endl;
    m_fd_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (m_fd_socket < 0) {
        perror("socket");
        throw 1;
    }

    sockaddr_un socket_address{};
    socket_address.sun_family = AF_UNIX;
    strncpy(socket_address.sun_path, m_path.c_str(), sizeof(socket_address.sun_path));
    std::cout << socket_address.sun_path << std::endl;

    if (bind(m_fd_socket, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address)) != 0)
    {
        perror("bind");
        throw 1;
    }
    epoll.register_fd(m_fd_socket, [this]() {
        std::cout << "IPC event" << std::endl;
        char buf[256];
        ssize_t bytes_read = read(m_fd_socket, buf, sizeof(buf) - 1);
        if (bytes_read < 0) {
            perror("read");
            throw 1;
        }

        // Protect from read out of bounds data
        buf[bytes_read + 1] = 0;

        m_message_handler(std::string_view(buf, static_cast<size_t>(bytes_read)));
    });
}

IPCServer::~IPCServer() {
    close(m_fd_socket);
    unlink(m_path.c_str());
}

void IPCServer::register_on_message_handler(std::function<void(const std::string_view)> message_handler) {
    m_message_handler = message_handler;
}
