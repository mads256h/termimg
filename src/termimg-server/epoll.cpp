//
// Created by mads on 02/03/2022.
//

#include "epoll.h"

#include <cassert>
#include <unistd.h>
#include <sys/epoll.h>


Epoll::Epoll() : m_fd_epoll(epoll_create(1)) {
}

Epoll::~Epoll() {
    close(m_fd_epoll);
}

void Epoll::register_fd(int fd, std::function<void()> event_handler) {
    m_event_handlers.insert({fd, event_handler});

    epoll_event new_event;
    new_event.data.fd = fd;
    new_event.events = EPOLLIN;

    if (epoll_ctl(m_fd_epoll, EPOLL_CTL_ADD, fd, &new_event) != 0) {
        // Do nothing right now
        throw 1;
    }
}

void Epoll::run_loop() const {
    epoll_event events[max_events];
    while(!m_should_exit) {
        int ready_fds = epoll_wait(m_fd_epoll, events, sizeof(events) / sizeof(events[0]), -1);
        for (int i = 0; i < ready_fds; ++i) {
            int fd = events[i].data.fd;
            assert(m_event_handlers.count(fd) == 1);
            m_event_handlers.at(fd)();
        }
    }
}

void Epoll::exit_loop() {
    m_should_exit = true;
}
