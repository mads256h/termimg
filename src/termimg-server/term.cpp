//
// Created by mads on 01/03/2022.
//

#include "term.h"

#include <ranges>
#include <vector>
#include <tuple>
#include <optional>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <cstdio>

#include <unistd.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/extensions/XRes.h>
#include <proc/readproc.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>


int get_pty(pid_t pid) {
    pid_t pids[] = {pid, 0};
    PROCTAB* proctab = openproc(PROC_FILLSTAT | PROC_PID, pids);

    proc_t* p = readproc(proctab, nullptr);

    auto tty = static_cast<dev_t>(p->tty);
    auto minor_tty = minor(tty);
    std::cerr << "tty " << tty << " minor " << minor_tty << std::endl;

    std::stringstream ss;
    ss << "/dev/pts/" << minor_tty;

    const auto path = ss.str();
    if (minor_tty != 0) {
        auto fd = open(path.c_str(), O_RDWR);
        if (fd == -1) {
            perror("open");
        }
        else {
            return fd;
        }
    }

    struct stat s{};
    if (stat(path.c_str(), &s) == -1){
        freeproc(p);

        closeproc(proctab);
        perror("stat");
        std::cerr << "Could not find pty for " << pid << std::endl;
        return -1;
    }

    freeproc(p);

    closeproc(proctab);

    if (tty == s.st_rdev) {
        std::cerr << "Found pty for " << pid << " with tty: " << tty << std::endl;
        return open(path.c_str(), O_RDWR);
    }

    return -1;
}

std::vector<pid_t> get_parent_pids(pid_t pid){
    std::vector<pid_t> parent_pids;
    parent_pids.push_back(pid);
    while (pid != 1) {
        pid_t pids[] = {pid, 0};
        PROCTAB* proctab = openproc(PROC_FILLSTAT | PROC_PID, pids);

        proc_t* p = readproc(proctab, nullptr);

        pid = p->ppid;
        if (pid != 1)
            parent_pids.push_back(pid);

        freeproc(p);

        closeproc(proctab);
    }

    return parent_pids;
}

bool has_property(std::shared_ptr<Display> display, Window window, Atom atom){
    Atom actual_type_return;
    int actual_format_return;
    unsigned long nitems_return;
    unsigned long bytes_after_return;
    unsigned char* prop_return;
    int status = XGetWindowProperty(
            display.get(),
            window,
            atom,
            0L,
            0L,
            False,
            AnyPropertyType,
            &actual_type_return,
            &actual_format_return,
            &nitems_return,
            &bytes_after_return,
            &prop_return);
    if (status == Success){
        XFree(prop_return);

        return actual_type_return != None && actual_format_return != 0;
    }

    return false;
}

void get_all_windows(std::shared_ptr<Display> display, Window window, std::vector<Window> &windows) {
    Atom wm_class = XInternAtom(display.get(), "WM_CLASS", True);
    Atom wm_name = XInternAtom(display.get(), "WM_NAME", True);
    Atom wm_locale_name = XInternAtom(display.get(), "WM_LOCALE_NAME", True);
    Atom wm_normal_hints = XInternAtom(display.get(), "WM_NORMAL_HINTS", True);

    Window unused;
    Window *children;
    unsigned int children_count = 0;
    XQueryTree(
            display.get(),
            window,
            &unused,
            &unused,
            &children,
            &children_count);

    if (children) {
        for (unsigned int i = 0; i < children_count; ++i) {
            auto child = children[i];
            auto p = [&](Atom atom) {
                return has_property(display, child, atom);
            };
            if (p(wm_class)
                && p(wm_name)
                && p(wm_locale_name)
                && p(wm_normal_hints)
                    )
            {
                windows.push_back(child);
            }
            get_all_windows(display, child, windows);
        }

        XFree(children);
    }
}

std::vector<Window> get_all_windows(std::shared_ptr<Display> display) {
    std::vector<Window> windows;
    Screen *screen = XDefaultScreenOfDisplay(display.get());
    get_all_windows(display, XRootWindowOfScreen(screen), windows);
    return windows;
}

pid_t get_window_pid(std::shared_ptr<Display> display, Window window) {
    XResClientIdSpec client_spec;
    client_spec.client = window;
    client_spec.mask = XRES_CLIENT_ID_PID_MASK;
    long num_ids = 0;
    XResClientIdValue *client_ids;


    XResQueryClientIds(display.get(), 1, &client_spec, &num_ids, &client_ids);

    pid_t pid = 0;
    for (long i = 0; i < num_ids; ++i) {
        XResClientIdValue *client_id = &client_ids[i];
        XResClientIdType client_id_type = XResGetClientIdType(client_id);

        if (client_id_type == XRES_CLIENT_ID_PID) {
            pid = XResGetClientPid(client_id);
            break;
        }
    }

    XResClientIdsDestroy(num_ids, client_ids);

    return pid;
}

std::optional<TerminalInfo> get_term(std::shared_ptr<Display> display, pid_t parent) {
    auto parent_pids = get_parent_pids(parent);

    int fd_pty = 0;

    for (int & parent_pid : std::ranges::reverse_view(parent_pids)) {
        std::cerr << parent_pid << std::endl;
        auto p = get_pty(parent_pid);
        if (p != -1) {
            fd_pty = p;
            break;
        }
    }

    auto all_windows = get_all_windows(display);
    for (auto window : all_windows){
        pid_t pid = get_window_pid(display, window);
        if(std::find(parent_pids.begin(), parent_pids.end(), pid) != parent_pids.end()) {
            std::cerr << "pty is " << fd_pty << std::endl;
            return TerminalInfo{window, fd_pty};
        }
    }


    return std::nullopt;
}
