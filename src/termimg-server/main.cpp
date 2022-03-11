#include <iostream>
#include <vector>
#include <map>
#include <string_view>
#include <charconv>
#include <sstream>

#include <cassert>
#include <cstdint>

#include <X11/Xlib.h>
#include <Imlib2.h>

#include <unistd.h>
#include <csignal>
#include <sys/signalfd.h>

#include "term.h"
#include "epoll.h"
#include "ipc-server.h"

const char* signal_to_string(uint32_t signal){
    switch(signal) {
        case SIGHUP: return "SIGHUP";
        case SIGINT: return "SIGINT";
        case SIGQUIT: return "SIGQUIT";
        case SIGTERM: return "SIGTERM";
        default: return "Unknown signal";
    }
}

void print_usage(const char* argv0) {
    std::cerr << "USAGE: " << argv0 << ": <parent_pid>" << std::endl;
}
std::vector<std::string_view> split (const std::string_view s, const std::string_view delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string_view token;
    std::vector<std::string_view> res;

    while ((pos_end = s.find (delimiter, pos_start)) != std::string_view::npos) {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (s.substr (pos_start));
    return res;
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

const char* get_imlib_load_error(Imlib_Load_Error load_error) {
    switch (load_error) {
        case IMLIB_LOAD_ERROR_NONE: return "Error none";
        case IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST: return "File does not exist";
        case IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY: return "File is directory";
        case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ: return "Permission denied to read";
        case IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT: return "No loader for file for file format";
        case IMLIB_LOAD_ERROR_PATH_TOO_LONG: return "Path too long";
        case IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT: return "Path component non existant";
        case IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY: return "Path component not directory";
        case IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE: return "Path points outside address space";
        case IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS: return "Too many symbolic links";
        case IMLIB_LOAD_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS: return "Out of file descriptors";
        case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE: return "Permission denied to write";
        case IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE: return "Out of disk space";
        case IMLIB_LOAD_ERROR_UNKNOWN: return "Unknown error";
        case IMLIB_LOAD_ERROR_IMAGE_READ: return "Image read";
        case IMLIB_LOAD_ERROR_IMAGE_FRAME: return "Image frame";
        default: return "WTF!";
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    pid_t parent_pid = std::stoi(argv[1]);

    Display *display = XOpenDisplay(nullptr);

    auto optional_term_tuple = get_term(display, parent_pid);
    if (!optional_term_tuple.has_value()) {
        std::cerr << "No terminal found!" << std::endl;
        sleep(100000);
        return EXIT_FAILURE;
    }

    const auto terminal_info = optional_term_tuple.value();

    //std::cerr << "Found terminal pid: " << term_pid << " window: " << std::hex << term_window << std::dec << " pty: " << pty << std::endl;



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
        terminal_info.terminal_window(),
        0, 0,
        50, 50,
        1,
        DefaultDepth(display, screen),
        InputOutput,
        XDefaultVisual(display, screen),
        CWEventMask | CWBackPixel | CWColormap | CWBorderPixel,
        &attributes
    );


    int fd_xorg = XConnectionNumber(display);

    Epoll epoll;
    epoll.register_fd(fd_xorg, [&]() {
        XEvent x_event;
        XNextEvent(display, &x_event);

        if (x_event.type == Expose){
            std::cerr << "Expose" << std::endl;
        }
        else {
            std::cerr << "Unknown event" << std::endl;
        }
    });

    Pixmap pixmap = -1ul;

    IPCServer ipc_server("/tmp/termimg", epoll);
    ipc_server.register_on_message_handler([&](const std::string_view message) {
        if (message == "clear") {
            XUnmapWindow(display, window);
            XFlush(display);
        }
        else if (message == "quit") {
            epoll.exit_loop();
        }
        else {
            auto strs = split(message, "\n");
            if (strs.size() != 6) {
                std::cerr << "Not enough arguments" << std::endl;
                return;
            }

            auto command = strs[0];
            if (command != "display") {
                std::cerr << "Unrecognized command" << std::endl;
                return;
            }

            auto get_int = [](std::string_view str) {
                int number = 0;
                const auto result = std::from_chars(str.data(), str.data() + str.size(), number, 10);

                if (result.ec == std::errc::invalid_argument) {
                    std::cerr << "Not an int" << std::endl;
                    throw 1;
                }

                return number;
            };

            const auto optional_winsize = terminal_info.get_tty_size();
            if (!optional_winsize.has_value()) {
                std::cerr << "Could not get tty size" << std::endl;
                throw 1;
            }
            const auto &winsize = optional_winsize.value();
            const auto columns = winsize.ws_col;
            const auto lines = winsize.ws_row;
            std::cerr << "Terminal size x: " << columns << " y: " << lines << std::endl;
            XWindowAttributes attr {};
            if (!XGetWindowAttributes(display, terminal_info.terminal_window(), &attr)) {
                std::cerr << "Could not get window attributes" << std::endl;
            }
            const int win_width = attr.width;
            const int win_height = attr.height;
            std::cerr << "Window width: " << win_width << " height: " << win_height << std::endl;
            const int x = static_cast<int>(static_cast<float>(get_int(strs[1])) * (static_cast<float>(win_width) / static_cast<float>(columns)));
            const int y = get_int(strs[2]) * (win_height / lines);
            int max_width = get_int(strs[3]) * (win_width / columns);
            int max_height = get_int(strs[4]) * (win_height / lines);

            if (max_width == 0) max_width = INT32_MAX;
            if (max_height == 0) max_height = INT32_MAX;

            std::string path = std::string(strs[5]);


            std::cerr << "Got message with x: " << x << " y: " << y << " max_width: " << max_width << " max_height: " << max_height << " path: " << path << std::endl;

            Imlib_Load_Error load_error;
            Imlib_Image image = imlib_load_image_with_error_return(path.data(), &load_error);
            if (!image) {
                std::cerr << "Image loading failed for image " << path <<  std::endl;
                std::cerr << get_imlib_load_error(load_error) << std::endl;
                return;
            }

            imlib_context_set_image(image);
            std::cerr << "Original width: " << imlib_image_get_width() << " height: " << imlib_image_get_height() << std::endl;

            Imlib_Image cropped_image = scale_image(image, max_width, max_height);
            imlib_free_image();
            imlib_context_set_image(cropped_image);

            const int img_width = imlib_image_get_width();
            const int img_height = imlib_image_get_height();

            assert(img_width >= 0);
            assert(img_height >= 0);

            const auto width = static_cast<unsigned int>(img_width);
            const auto height = static_cast<unsigned int>(img_height);

            std::cerr << "Cropped width: " << width << " height: " << height << std::endl;

            if (pixmap != -1ul) {
                XFreePixmap(display, pixmap);
                pixmap = -1ul;
            }

            pixmap = XCreatePixmap(display, window, width, height, static_cast<unsigned int>(DefaultDepth(display, screen)));
            XResizeWindow(display, window, width, height);
            XMoveWindow(display, window, x, y);

            imlib_context_set_display(display);
            imlib_context_set_visual(DefaultVisual(display, screen));
            imlib_context_set_colormap(DefaultColormap(display, screen));
            imlib_context_set_drawable(pixmap);

            imlib_render_image_on_drawable(0, 0);
            imlib_free_image();

            XSetWindowBackgroundPixmap(display, window, pixmap);
            XUnmapWindow(display, window);
            XMapRaised(display, window);
            XFlushGC(display, DefaultGC(display, screen));
            XFlush(display);
        }
    });

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &mask, 0) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }

    int fd_signal = signalfd(-1, &mask, 0);
    if (fd_signal == -1) {
        perror("signalfd");
        return EXIT_FAILURE;
    }

    epoll.register_fd(fd_signal, [&]() {
        struct signalfd_siginfo siginfo{};
        if (read(fd_signal, &siginfo, sizeof(siginfo)) == -1) {
            perror("read");
            throw 1;
        }

        std::cerr << "Got signal " << signal_to_string(siginfo.ssi_signo) << std::endl;
        epoll.exit_loop();
    });

    XSelectInput(display, window, ExposureMask);

    epoll.run_loop();

    if (pixmap != -1ul) {
        XFreePixmap(display, pixmap);
    }
    XCloseDisplay(display);
    return EXIT_SUCCESS;
}
