#include <iostream>
#include <vector>
#include <map>
#include <X11/Xlib.h>
#include <X11/extensions/XRes.h>
#include <Imlib2.h>
#include <cassert>
#include <proc/readproc.h>

void print_usage(const char* argv0) {
    std::cerr << "USAGE: " << argv0 << ": <x> <y> <max_width> <max_height> <image>" << std::endl;
}

Imlib_Image scale_image(Imlib_Image image, int max_width, int max_height) {
    imlib_context_set_image(image);

    const int img_width = imlib_image_get_width();
    const int img_height = imlib_image_get_height();

    const float aspect_ratio = static_cast<float>(img_width) / static_cast<float>(img_height);
    const float aspect_ratio_inverse = 1.0f / aspect_ratio;

    const int raw_width = std::min(img_width, max_width);
    const int raw_height = std::min(img_height, max_height);

    const int aspect_corrected_width = std::min(raw_width, static_cast<int>(static_cast<float>(raw_height) * aspect_ratio));
    const int aspect_corrected_height = std::min(raw_height, static_cast<int>(static_cast<float>(raw_width) * aspect_ratio_inverse));

    return imlib_create_cropped_scaled_image(0, 0, img_width, img_height, aspect_corrected_width, aspect_corrected_height);
}

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
    //std::sort(windows.begin(), windows.end());
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

int main(int argc, char* argv[]) {
    Display *display = XOpenDisplay(nullptr);

    auto parent_pids = get_parent_pids();
    std::cout << "Printing pids..." << std::endl;
    for (auto pid : parent_pids) {
        std::cout << pid << std::endl;
    }

    Window term_window = 0;

    auto all_windows = get_all_windows(display);
    std::cout << "Printing windows..." << std::endl;
    for (auto window : all_windows){
        pid_t pid = get_window_pid(display, window);
        if(std::find(parent_pids.begin(), parent_pids.end(), pid) != parent_pids.end()) {
            std::cout << std::dec << "Pid: " << pid << " Window: " << std::hex << window << std::dec << std::endl;
            term_window = window;
            break;
        }
    }


    if (argc != 6) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const int x = std::stoi(argv[1]);
    const int y = std::stoi(argv[2]);
    const int max_width = std::stoi(argv[3]);
    const int max_height = std::stoi(argv[4]);


    Imlib_Image image = imlib_load_image(argv[5]);
    if (!image) {
        std::cout << "Image loading failed!" << std::endl;
        return EXIT_FAILURE;
    }

    imlib_context_set_image(image);
    std::cout << "Original width: " << imlib_image_get_width() << " height: " << imlib_image_get_height() << std::endl;

    Imlib_Image cropped_image = scale_image(image, max_width, max_height);
    imlib_free_image();
    imlib_context_set_image(cropped_image);

    const int img_width = imlib_image_get_width();
    const int img_height = imlib_image_get_height();

    assert(img_width >= 0);
    assert(img_height >= 0);

    const auto width = static_cast<unsigned int>(img_width);
    const auto height = static_cast<unsigned int>(img_height);

    std::cout << "Cropped width: " << width << " height: " << height << std::endl;

    const int screen = DefaultScreen(display);
    XSetWindowAttributes attributes;
    attributes.event_mask = ExposureMask;
    attributes.colormap = XCreateColormap(
            display, XDefaultRootWindow(display),
            XDefaultVisual(display, screen), AllocNone);
    attributes.background_pixel = 0;
    attributes.border_pixel = 0;
    Window window = XCreateWindow(
        display,
        term_window,
        x, y,
        width, height,
        1,
        DefaultDepth(display, screen),
        InputOutput,
        XDefaultVisual(display, screen),
        CWEventMask | CWBackPixel | CWColormap | CWBorderPixel,
        &attributes
    );

    Pixmap pixmap = XCreatePixmap(display, window, width, height, static_cast<unsigned int>(DefaultDepth(display, screen)));
    imlib_context_set_display(display);
    imlib_context_set_visual(DefaultVisual(display, screen));
    imlib_context_set_colormap(DefaultColormap(display, screen));
    imlib_context_set_drawable(pixmap);

    imlib_render_image_on_drawable(0, 0);

    XSetWindowBackgroundPixmap(display, window, pixmap);

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);

    while(true) {
        XEvent event;
        XNextEvent(display, &event);

        if (event.type == KeyPress) {
            if (event.xkey.keycode == 24)
                break;
        }

        if (event.type == Expose){
            std::cout << "Expose" << std::endl;
        }
    }

    XFreePixmap(display, pixmap);
    imlib_free_image();
    XCloseDisplay(display);
    return 0;
}
