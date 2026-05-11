#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    Window frame;
    Window title;
    Window content;
    Window child;
    GC title_gc;
    pid_t child_pid;
    int title_height;
    char title_text[256];
} AppWindow;

static AppWindow render_window(
    Display *display,
    Window root,
    int screen,
    Colormap colormap,
    const char *application
) {
    const unsigned int width = 900;
    const unsigned int height = 600;
    const int title_height = 28;

    const Window frame = XCreateSimpleWindow(
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

    XColor title_bg = {0};
    unsigned long title_bg_pixel = WhitePixel(display, screen);
    if (XParseColor(display, colormap, "#2b2f36", &title_bg) != 0) {
        if (XAllocColor(display, colormap, &title_bg) != 0) {
            title_bg_pixel = title_bg.pixel;
        }
    }

    const Window title = XCreateSimpleWindow(
        display,
        frame,
        0,
        0,
        width,
        (unsigned int)title_height,
        0,
        BlackPixel(display, screen),
        title_bg_pixel
    );

    const Window content = XCreateSimpleWindow(
        display,
        frame,
        0,
        title_height,
        width,
        height - title_height,
        0,
        BlackPixel(display, screen),
        WhitePixel(display, screen)
    );

    XSelectInput(display, frame, StructureNotifyMask);
    XSelectInput(display, title, ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XSelectInput(display, content, ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(display, title);
    XMapWindow(display, content);
    XMapWindow(display, frame);
    XFlush(display);

    const char *app = (application != NULL && application[0] != '\0') ? application : "/usr/bin/xterm";
    pid_t child = fork();
    if (child == 0) {
        char window_id[32];
        snprintf(window_id, sizeof(window_id), "%lu", (unsigned long)content);
        execl(app, app, "-into", window_id, (char *)NULL);
        _exit(EXIT_FAILURE);
    }

    AppWindow result = {0};
    result.frame = frame;
    result.title = title;
    result.content = content;
    result.child = None;
    result.title_gc = XCreateGC(display, title, 0, NULL);
    result.child_pid = child;
    result.title_height = title_height;
    snprintf(result.title_text, sizeof(result.title_text), "%s", app);

    for (int i = 0; i < 30 && result.child == None; i++) {
        Window root_return = None;
        Window parent_return = None;
        Window *children = NULL;
        unsigned int nchildren = 0;
        if (XQueryTree(display, content, &root_return, &parent_return, &children, &nchildren) != 0) {
            if (nchildren > 0) {
                result.child = children[0];
            }
        }
        if (children != NULL) {
            XFree(children);
        }
        if (result.child != None) {
            XResizeWindow(display, result.child, width, height - title_height);
            break;
        }
        usleep(100000);
    }

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

    AppWindow app = render_window(display, root, screen, colormap, "/usr/bin/xterm");

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

            if (event.type == Expose && event.xany.window == app.title && event.xexpose.count == 0) {
                XClearWindow(display, app.title);
                if (app.title_gc != NULL) {
                    XSetForeground(display, app.title_gc, BlackPixel(display, screen));
                    XDrawString(
                        display,
                        app.title,
                        app.title_gc,
                        10,
                        app.title_height - 10,
                        app.title_text,
                        (int)strlen(app.title_text)
                    );
                }
            } else if (event.type == ButtonPress && event.xany.window == app.title && event.xbutton.button == Button1) {
                XWindowAttributes attrs;
                XGetWindowAttributes(display, app.frame, &attrs);
                dragging = 1;
                drag_start_x = event.xbutton.x_root;
                drag_start_y = event.xbutton.y_root;
                win_start_x = attrs.x;
                win_start_y = attrs.y;
            } else if (event.type == MotionNotify && dragging) {
                int dx = event.xmotion.x_root - drag_start_x;
                int dy = event.xmotion.y_root - drag_start_y;
                XMoveWindow(display, app.frame, win_start_x + dx, win_start_y + dy);
            } else if (event.type == ButtonRelease && event.xany.window == app.title && event.xbutton.button == Button1) {
                dragging = 0;
            } else if (event.type == ButtonPress && event.xany.window == app.content) {
                Window focus_target = (app.child != None) ? app.child : app.content;
                XSetInputFocus(display, focus_target, RevertToParent, CurrentTime);
            } else if (event.type == ConfigureNotify && event.xany.window == app.frame) {
                int new_width = event.xconfigure.width;
                int new_height = event.xconfigure.height;
                int content_height = new_height - app.title_height;
                if (content_height < 1) {
                    content_height = 1;
                }
                XResizeWindow(display, app.title, (unsigned int)new_width, (unsigned int)app.title_height);
                XMoveResizeWindow(
                    display,
                    app.content,
                    0,
                    app.title_height,
                    (unsigned int)new_width,
                    (unsigned int)content_height
                );
                if (app.child != None) {
                    XResizeWindow(display, app.child, (unsigned int)new_width, (unsigned int)content_height);
                }
            }
        }

        usleep(10000);
    }

    XFreeCursor(display, cursor);
    XCloseDisplay(display);
    return EXIT_SUCCESS;
}
