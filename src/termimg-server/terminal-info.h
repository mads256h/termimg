//
// Created by mads on 10/03/2022.
//

#ifndef TERMIMG_TERMINAL_INFO_H
#define TERMIMG_TERMINAL_INFO_H

#include <optional>

#include <sys/ioctl.h>

#include <X11/Xlib.h>


class TerminalInfo {
    Window m_terminal_window;
    int m_fd_pty;

public:
    TerminalInfo(Window terminal_window, int fd_pty);
    TerminalInfo(const TerminalInfo&) = default;
    TerminalInfo(TerminalInfo&&) = default;

    [[nodiscard]] Window terminal_window() const;

    [[nodiscard]] std::optional<winsize> get_tty_size() const;

};


#endif //TERMIMG_TERMINAL_INFO_H
