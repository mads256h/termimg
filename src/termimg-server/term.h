//
// Created by mads on 01/03/2022.
//

#ifndef TERMIMG_TERM_H
#define TERMIMG_TERM_H

#include <tuple>
#include <optional>
#include <unistd.h>
#include <X11/Xlib.h>

std::tuple<int, int> get_tty_size(int fd);

std::optional<std::tuple<pid_t,Window,int>> get_term(Display *display, pid_t parent);



#endif //TERMIMG_TERM_H
