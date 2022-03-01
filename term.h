//
// Created by mads on 01/03/2022.
//

#ifndef TERMIMG_TERM_H
#define TERMIMG_TERM_H

#include <tuple>
#include <optional>
#include <unistd.h>
#include <X11/Xlib.h>

std::optional<std::tuple<pid_t,Window>> get_term(Display *display);



#endif //TERMIMG_TERM_H
