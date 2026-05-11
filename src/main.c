#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>

#include <X11/Xlib.h>
#include <X11/cursorfont.h>

static volatile sig_atomic_t keep_running = 1;

#define XCursorIconPath "~/hatde/src/assets/mouse.svg"
#define XfontPath "~/hatde/src/assets/JetBrainsMonoNerdFont-Regular.ttf"

#define XBackgroundColor "#00a2ff"

static void stop_running(int signo) {
    (void)signo;
    keep_running = 0;
}

typedef struct {
    Window container;
    pid_t child_pid;
} AppWindow;

static AppWindow render_window(Display *display, Window root, int screen, const char *application) {
    const unsigned int width = 900;
    const unsigned int height = 600;

    const Window container = XCreateSimpleWindow(
        display,
        root,
        100,
        100,
        width,
        height,
        1,
        BlackPixel(display, screen),
        WhitePixel(display, screen)
    );

    XSelectInput(display, container, ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(display, container);
    XFlush(display);

    const char *app = (application != NULL && application[0] != '\0') ? application : "/usr/bin/xterm";
    pid_t child = fork();
    if (child == 0) {
        char window_id[32];
        snprintf(window_id, sizeof(window_id), "%lu", (unsigned long)container);
        execl(app, app, "-into", window_id, (char *)NULL);
        _exit(EXIT_FAILURE);
    }

    AppWindow result = {0};
    result.container = container;
    result.child_pid = child;
    return result;
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
    if (XParseColor(display, colormap, XBackgroundColor, &background) == 0) {
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

    AppWindow app = render_window(display, root, screen, "/usr/bin/xterm");

    XDefineCursor(display, root, cursor);
    XFlush(display);

    signal(SIGINT, stop_running);
    signal(SIGTERM, stop_running);

    int dragging = 0;
    int drag_start_x = 0;
    int drag_start_y = 0;
    int win_start_x = 0;
    int win_start_y = 0;

    while (keep_running) {
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);

            if (event.xany.window != app.container) {
                continue;
            }

            if (event.type == ButtonPress && event.xbutton.button == Button1) {
                XWindowAttributes attrs;
                XGetWindowAttributes(display, app.container, &attrs);
                dragging = 1;
                drag_start_x = event.xbutton.x_root;
                drag_start_y = event.xbutton.y_root;
                win_start_x = attrs.x;
                win_start_y = attrs.y;
            } else if (event.type == MotionNotify && dragging) {
                int dx = event.xmotion.x_root - drag_start_x;
                int dy = event.xmotion.y_root - drag_start_y;
                XMoveWindow(display, app.container, win_start_x + dx, win_start_y + dy);
            } else if (event.type == ButtonRelease && event.xbutton.button == Button1) {
                dragging = 0;
            }
        }

        usleep(10000);
    }

    XFreeCursor(display, cursor);
    XCloseDisplay(display);
    return EXIT_SUCCESS;
}
