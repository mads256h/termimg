//
// Created by mads on 02/03/2022.
//

#ifndef TERMIMG_EPOLL_H
#define TERMIMG_EPOLL_H

#include <map>
#include <functional>


class Epoll {
private:
    const int m_fd_epoll;
    std::map<int, std::function<void()>> m_event_handlers;
    bool m_should_exit = false;

    // Arbitrary
    constexpr static int max_events = 10;

public:
    Epoll();
    Epoll(Epoll&) = delete;
    ~Epoll();

    void register_fd(int fd, std::function<void()>);
    void run_loop() const;
    void exit_loop();
};


#endif //TERMIMG_EPOLL_H
