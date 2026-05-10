#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/cursorfont.h>

static volatile sig_atomic_t keep_running = 1;

static void stop_running(int signo) {
    (void)signo;
    keep_running = 0;
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "hatde: failed to open X display\n");
        return EXIT_FAILURE;
    }

    const int screen = DefaultScreen(display);
    const Window root = RootWindow(display, screen);
    const Colormap colormap = DefaultColormap(display, screen);

    XColor background = {0};
    if (XParseColor(display, colormap, "#00a2ff", &background) == 0) {
        fprintf(stderr, "hatde: invalid background color\n");
        XCloseDisplay(display);
        return EXIT_FAILURE;
    }

    if (XAllocColor(display, colormap, &background) == 0) {
        fprintf(stderr, "hatde: failed to allocate background color\n");
        XCloseDisplay(display);
        return EXIT_FAILURE;
    }

    XSetWindowBackground(display, root, background.pixel);
    XClearWindow(display, root);

    const Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
    if (cursor == None) {
        fprintf(stderr, "hatde: failed to create mouse cursor\n");
        XCloseDisplay(display);
        return EXIT_FAILURE;
    }

    XDefineCursor(display, root, cursor);
    XFlush(display);

    signal(SIGINT, stop_running);
    signal(SIGTERM, stop_running);

    while (keep_running) {
        pause();
    }

    XFreeCursor(display, cursor);
    XCloseDisplay(display);
    return EXIT_SUCCESS;
}
