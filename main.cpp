#include <iostream>
#include <X11/Xlib.h>
#include <Imlib2.h>
#include <cassert>

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

    Display *display = XOpenDisplay(nullptr);
    const int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(
            display,
            RootWindow(display, screen),
            x, y,
            width, height,
            1,
            BlackPixel(display, screen),
            WhitePixel(display, screen));

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
