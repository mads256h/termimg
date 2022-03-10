//
// Created by mads on 10/03/2022.
//

#include <cstdio>

#include "terminal-info.h"

TerminalInfo::TerminalInfo(Window terminal_window, int fd_pty) : m_terminal_window(terminal_window), m_fd_pty(fd_pty) { }

std::optional<winsize> TerminalInfo::get_tty_size() const {
    winsize win_size{};
    if (ioctl(m_fd_pty, TIOCGWINSZ, &win_size) == -1) {
        perror("ioctl");
        return std::nullopt;
    }

    return win_size;
}

Window TerminalInfo::terminal_window() const {
    return m_terminal_window;
}
