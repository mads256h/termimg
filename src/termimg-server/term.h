//
// Created by mads on 01/03/2022.
//

#ifndef TERMIMG_TERM_H
#define TERMIMG_TERM_H

#include <tuple>
#include <optional>
#include <memory>

#include <unistd.h>
#include <X11/Xlib.h>

#include "terminal-info.h"

std::optional<TerminalInfo> get_term(std::shared_ptr<Display> display, pid_t parent);



#endif //TERMIMG_TERM_H
