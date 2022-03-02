#include <iostream>
#include <vector>
#include <map>
#include <string_view>

#include <cassert>
#include <cstdint>

#include <X11/Xlib.h>
#include <Imlib2.h>

#include "term.h"
#include "epoll.h"
#include "ipc-server.h"

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


int main(int argc, char* argv[]) {
    Display *display = XOpenDisplay(nullptr);

    auto optional_term_tuple = get_term(display);
    if (!optional_term_tuple.has_value()) {
        std::cerr << "No terminal found!" << std::endl;
        return EXIT_FAILURE;
    }

    auto [term_pid, term_window] = optional_term_tuple.value();

    std::cout << "Found terminal pid: " << term_pid << " window: " << std::hex << term_window << std::dec << std::endl;

    if (argc != 6) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const int x = std::stoi(argv[1]);
    const int y = std::stoi(argv[2]);
    int max_width = std::stoi(argv[3]);
    if (max_width == 0) max_width = INT32_MAX;
    int max_height = std::stoi(argv[4]);
    if (max_height == 0) max_height = INT32_MAX;


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


    int fd_xorg = XConnectionNumber(display);

    Epoll epoll;
    epoll.register_fd(fd_xorg, [&]() {
        std::cout << "X event" << std::endl;
        XEvent x_event;
        XNextEvent(display, &x_event);

        if (x_event.type == KeyPress) {
            if (x_event.xkey.keycode == 24)
                epoll.exit_loop();
        }

        if (x_event.type == Expose){
            std::cout << "Expose" << std::endl;
        }
    });


    IPCServer ipc_server("/tmp/termimg", epoll);
    ipc_server.register_on_message_handler([](const std::string_view message) {
        std::cout << message << std::endl;
    });


    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);
    XFlush(display);

    epoll.run_loop();

    XFreePixmap(display, pixmap);
    imlib_free_image();
    XCloseDisplay(display);
    return 0;
}
