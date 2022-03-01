//
// Created by mads on 01/03/2022.
//

#include "term.h"

#include <vector>
#include <tuple>
#include <optional>
#include <iostream>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/XRes.h>
#include <proc/readproc.h>

std::vector<pid_t> get_parent_pids(){
    std::vector<pid_t> parent_pids;
    pid_t pid = getpid();
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

bool has_property(Display* display, Window window, Atom atom){
    Atom actual_type_return;
    int actual_format_return;
    unsigned long nitems_return;
    unsigned long bytes_after_return;
    unsigned char* prop_return;
    int status = XGetWindowProperty(
            display,
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

void get_all_windows(Display* display, Window window, std::vector<Window> &windows) {
    Atom wm_class = XInternAtom(display, "WM_CLASS", True);
    Atom wm_name = XInternAtom(display, "WM_NAME", True);
    Atom wm_locale_name = XInternAtom(display, "WM_LOCALE_NAME", True);
    Atom wm_normal_hints = XInternAtom(display, "WM_NORMAL_HINTS", True);

    Window unused;
    Window *children;
    unsigned int children_count = 0;
    XQueryTree(
            display,
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

std::vector<Window> get_all_windows(Display* display) {
    std::vector<Window> windows;
    Screen *screen = XDefaultScreenOfDisplay(display);
    get_all_windows(display, XRootWindowOfScreen(screen), windows);
    return windows;
}

pid_t get_window_pid(Display *display, Window window) {
    XResClientIdSpec client_spec;
    client_spec.client = window;
    client_spec.mask = XRES_CLIENT_ID_PID_MASK;
    long num_ids = 0;
    XResClientIdValue *client_ids;


    XResQueryClientIds(display, 1, &client_spec, &num_ids, &client_ids);

    pid_t pid = 0;
    for (long i = 0; i < num_ids; ++i) {
        XResClientIdValue *client_id = &client_ids[i];
        XResClientIdType client_id_type = XResGetClientIdType(client_id);

        if (client_id_type == XRES_CLIENT_ID_PID) {
            pid = XResGetClientPid(client_id);
            break;
        }
    }

    XFree(client_ids);

    return pid;
}

std::optional<std::tuple<pid_t, Window>> get_term(Display *display) {
    auto parent_pids = get_parent_pids();

    auto all_windows = get_all_windows(display);
    for (auto window : all_windows){
        pid_t pid = get_window_pid(display, window);
        if(std::find(parent_pids.begin(), parent_pids.end(), pid) != parent_pids.end()) {
            return std::make_tuple(pid, window);
        }
    }

    return std::nullopt;
}
