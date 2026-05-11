#include "app_window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

static void app_window_resize_content(Display *display, AppWindow *window, int width, int height) {
    int content_height = height - window->title_height;
    if (content_height < 1) {
        content_height = 1;
    }

    XResizeWindow(display, window->title, (unsigned int)width, (unsigned int)window->title_height);
    XMoveResizeWindow(
        display,
        window->content,
        0,
        window->title_height,
        (unsigned int)width,
        (unsigned int)content_height
    );

    if (window->child != None) {
        XResizeWindow(display, window->child, (unsigned int)width, (unsigned int)content_height);
    }
}

static void app_window_make_font_spec(const char *font_name, char *buffer, size_t size) {
    if (font_name == NULL || font_name[0] == '\0') {
        snprintf(buffer, size, "sans-12");
        return;
    }

    const char *ext = strrchr(font_name, '.');
    if (strstr(font_name, "/") != NULL && ext != NULL &&
        (strcmp(ext, ".ttf") == 0 || strcmp(ext, ".otf") == 0)) {
        snprintf(buffer, size, "file=%s:size=12", font_name);
        return;
    }

    snprintf(buffer, size, "%s", font_name);
}

AppWindow *app_window_create(
    Display *display,
    Window root,
    int screen,
    Colormap colormap,
    const char *title_font,
    int x,
    int y,
    unsigned int width,
    unsigned int height
) {
    AppWindow *window = calloc(1, sizeof(*window));
    if (window == NULL) {
        return NULL;
    }

    window->title_height = 28;

    window->frame = XCreateSimpleWindow(
        display,
        root,
        x,
        y,
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

    window->title = XCreateSimpleWindow(
        display,
        window->frame,
        0,
        0,
        width,
        (unsigned int)window->title_height,
        0,
        BlackPixel(display, screen),
        title_bg_pixel
    );

    window->content = XCreateSimpleWindow(
        display,
        window->frame,
        0,
        window->title_height,
        width,
        height - window->title_height,
        0,
        BlackPixel(display, screen),
        WhitePixel(display, screen)
    );

    window->title_gc = XCreateGC(display, window->title, 0, NULL);
    window->title_draw = NULL;
    window->title_font = NULL;
    memset(&window->title_color, 0, sizeof(window->title_color));

    char font_spec[256];
    app_window_make_font_spec(title_font, font_spec, sizeof(font_spec));
    window->title_draw = XftDrawCreate(
        display,
        window->title,
        DefaultVisual(display, screen),
        colormap
    );
    if (window->title_draw != NULL) {
        window->title_font = XftFontOpenName(display, screen, font_spec);
        if (window->title_font == NULL) {
            XftDrawDestroy(window->title_draw);
            window->title_draw = NULL;
        }
    }

    if (window->title_draw != NULL) {
        XftColorAllocName(
            display,
            DefaultVisual(display, screen),
            colormap,
            "#f5f5f5",
            &window->title_color
        );
    }

    XSelectInput(display, window->frame, StructureNotifyMask);
    XSelectInput(
        display,
        window->title,
        ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask
    );
    XSelectInput(display, window->content, ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    XMapWindow(display, window->title);
    XMapWindow(display, window->content);
    XMapWindow(display, window->frame);
    XFlush(display);

    return window;
}

void app_window_destroy(Display *display, AppWindow *window) {
    if (window == NULL) {
        return;
    }

    if (window->title_gc != NULL) {
        XFreeGC(display, window->title_gc);
    }

    if (window->title_draw != NULL) {
        XftDrawDestroy(window->title_draw);
    }

    if (window->title_font != NULL) {
        XftFontClose(display, window->title_font);
    }

    if (window->title_color.pixel != 0) {
        XftColorFree(
            display,
            DefaultVisual(display, DefaultScreen(display)),
            DefaultColormap(display, DefaultScreen(display)),
            &window->title_color
        );
    }

    if (window->frame != None) {
        XDestroyWindow(display, window->frame);
    }

    free(window);
}

void app_window_set_title(AppWindow *window, const char *title) {
    if (window == NULL) {
        return;
    }

    if (title == NULL || title[0] == '\0') {
        snprintf(window->title_text, sizeof(window->title_text), "hatDE");
        return;
    }

    snprintf(window->title_text, sizeof(window->title_text), "%s", title);
}

void app_window_set_title_from_child(Display *display, AppWindow *window) {
    if (window == NULL || window->child == None) {
        return;
    }

    char *child_name = NULL;
    if (XFetchName(display, window->child, &child_name) != 0 && child_name != NULL) {
        app_window_set_title(window, child_name);
        XFree(child_name);
    }
}

void app_window_redraw_title(Display *display, AppWindow *window, int screen) {
    if (window == NULL) {
        return;
    }

    XClearWindow(display, window->title);

    if (window->title_draw != NULL && window->title_font != NULL) {
        XftDrawStringUtf8(
            window->title_draw,
            &window->title_color,
            window->title_font,
            10,
            window->title_height - 8,
            (const FcChar8 *)window->title_text,
            (int)strlen(window->title_text)
        );
        return;
    }

    if (window->title_gc == NULL) {
        return;
    }

    XSetForeground(display, window->title_gc, WhitePixel(display, screen));
    XDrawString(
        display,
        window->title,
        window->title_gc,
        10,
        window->title_height - 10,
        window->title_text,
        (int)strlen(window->title_text)
    );
}

static void app_window_wait_for_child(Display *display, AppWindow *window) {
    for (int i = 0; i < 40 && window->child == None; i++) {
        Window root_return = None;
        Window parent_return = None;
        Window *children = NULL;
        unsigned int nchildren = 0;
        if (XQueryTree(display, window->content, &root_return, &parent_return, &children, &nchildren) != 0) {
            if (nchildren > 0) {
                window->child = children[0];
            }
        }
        if (children != NULL) {
            XFree(children);
        }
        if (window->child != None) {
            XWindowAttributes frame_attrs;
            XGetWindowAttributes(display, window->frame, &frame_attrs);
            XSelectInput(display, window->child, PropertyChangeMask);
            app_window_resize_content(display, window, frame_attrs.width, frame_attrs.height);
            break;
        }
        usleep(100000);
    }
}

void app_window_spawn_in_frame(Display *display, AppWindow *window, const char *application) {
    if (window == NULL) {
        return;
    }

    const char *app = (application != NULL && application[0] != '\0') ? application : "/usr/bin/xterm";
    pid_t child = fork();
    if (child == 0) {
        char window_id[32];
        snprintf(window_id, sizeof(window_id), "%lu", (unsigned long)window->content);
        execl(app, app, "-into", window_id, (char *)NULL);
        _exit(EXIT_FAILURE);
    }

    window->child_pid = child;
    app_window_set_title(window, app);
    app_window_wait_for_child(display, window);
}

void app_window_wrap_child(Display *display, AppWindow *window, Window child) {
    if (window == NULL || child == None) {
        return;
    }

    window->child = child;
    XSelectInput(display, window->child, PropertyChangeMask);
    XReparentWindow(display, window->child, window->content, 0, 0);
    XMapWindow(display, window->child);

    XWindowAttributes frame_attrs;
    XGetWindowAttributes(display, window->frame, &frame_attrs);
    app_window_resize_content(display, window, frame_attrs.width, frame_attrs.height);

    app_window_set_title_from_child(display, window);
}

int app_window_handle_event(AppWindow *window, Display *display, int screen, XEvent *event) {
    if (window == NULL || event == NULL) {
        return 0;
    }

    if (event->type == Expose && event->xany.window == window->title && event->xexpose.count == 0) {
        app_window_redraw_title(display, window, screen);
        return 1;
    }

    if (event->type == ButtonPress && event->xany.window == window->title && event->xbutton.button == Button1) {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, window->frame, &attrs);
        window->dragging = 1;
        window->drag_start_x = event->xbutton.x_root;
        window->drag_start_y = event->xbutton.y_root;
        window->win_start_x = attrs.x;
        window->win_start_y = attrs.y;
        XRaiseWindow(display, window->frame);
        return 1;
    }

    if (event->type == MotionNotify && window->dragging != 0) {
        int dx = event->xmotion.x_root - window->drag_start_x;
        int dy = event->xmotion.y_root - window->drag_start_y;
        XMoveWindow(display, window->frame, window->win_start_x + dx, window->win_start_y + dy);
        return 1;
    }

    if (event->type == ButtonRelease && event->xany.window == window->title && event->xbutton.button == Button1) {
        window->dragging = 0;
        return 1;
    }

    if (event->type == ButtonPress && event->xany.window == window->content) {
        Window focus_target = (window->child != None) ? window->child : window->content;
        XSetInputFocus(display, focus_target, RevertToParent, CurrentTime);
        XRaiseWindow(display, window->frame);
        return 1;
    }

    if (event->type == ConfigureNotify && event->xany.window == window->frame) {
        app_window_resize_content(display, window, event->xconfigure.width, event->xconfigure.height);
        return 1;
    }

    return 0;
}
