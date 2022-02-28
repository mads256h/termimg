#include <iostream>
#include <X11/Xlib.h>
#include <Imlib2.h>

int main() {
    Imlib_Image image = imlib_load_image("/home/mads/image.png");
    if (!image) {
        std::cout << "Image loading failed!";
        return EXIT_FAILURE;
    }

    imlib_context_set_image(image);
    int width = imlib_image_get_width();
    int height = imlib_image_get_height();

    Display *display = XOpenDisplay(nullptr);
    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(
            display,
            RootWindow(display, screen),
            10, 10,
            width, height,
            1,
            BlackPixel(display, screen),
            WhitePixel(display, screen));

    Pixmap pixmap = XCreatePixmap(display, window, width, height, DefaultDepth(display, screen));
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
    return 0;
}
