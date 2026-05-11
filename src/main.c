#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/xpm.h>

#include "app_window.h"
#include "config.h"

static volatile sig_atomic_t keep_running = 1;

#define XBackgroundColor "#00a2ff"

static void set_root_background(
    Display *display,
    Window root,
    Colormap colormap,
    const Config *config,
    Pixmap *background_pixmap
) {
    XColor background = {0};
    if (XParseColor(display, colormap, XBackgroundColor, &background) != 0) {
        if (XAllocColor(display, colormap, &background) != 0) {
            XSetWindowBackground(display, root, background.pixel);
        }
    }

    if (config != NULL && strcmp(config->background_mode, "image") == 0) {
        if (config->background_image[0] != '\0') {
            XpmAttributes attrs;
            memset(&attrs, 0, sizeof(attrs));
            attrs.valuemask = XpmReturnPixels;

            Pixmap pixmap = None;
            int result = XpmReadFileToPixmap(
                display,
                root,
                config->background_image,
                &pixmap,
                NULL,
                &attrs
            );
            if (result == XpmSuccess) {
                XSetWindowBackgroundPixmap(display, root, pixmap);
                if (background_pixmap != NULL) {
                    *background_pixmap = pixmap;
                }
            } else {
                fprintf(stderr, "hatde: failed to load background image\n");
            }
        }
    }

    XClearWindow(display, root);
}

static void stop_running(int signo) {
    (void)signo;
    keep_running = 0;
}

typedef struct {
    AppWindow **items;
    size_t count;
    size_t capacity;
    int next_x;
    int next_y;
} AppWindowList;

static void window_list_init(AppWindowList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    list->next_x = 80;
    list->next_y = 80;
}

static void window_list_add(AppWindowList *list, AppWindow *window) {
    if (list->count == list->capacity) {
        size_t new_capacity = (list->capacity == 0) ? 4 : list->capacity * 2;
        AppWindow **new_items = realloc(list->items, new_capacity * sizeof(*new_items));
        if (new_items == NULL) {
            return;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->count++] = window;
}

static int window_matches(AppWindow *window, Window handle) {
    return window->frame == handle || window->title == handle ||
        window->content == handle || window->child == handle;
}

static AppWindow *window_list_find(AppWindowList *list, Window handle) {
    for (size_t i = 0; i < list->count; i++) {
        if (window_matches(list->items[i], handle)) {
            return list->items[i];
        }
    }
    return NULL;
}

static void window_list_remove(AppWindowList *list, size_t index) {
    if (index >= list->count) {
        return;
    }

    for (size_t i = index + 1; i < list->count; i++) {
        list->items[i - 1] = list->items[i];
    }
    list->count--;
}

static int window_is_top_level(Display *display, Window root, Window window) {
    Window root_return = None;
    Window parent_return = None;
    Window *children = NULL;
    unsigned int nchildren = 0;
    int result = 0;

    if (XQueryTree(display, window, &root_return, &parent_return, &children, &nchildren) != 0) {
        result = (parent_return == root);
    }

    if (children != NULL) {
        XFree(children);
    }

    return result;
}

static AppWindow *wrap_window(
    AppWindowList *list,
    Display *display,
    Window root,
    int screen,
    Colormap colormap,
    Window child,
    int width,
    int height
) {
    int x = list->next_x;
    int y = list->next_y;

    list->next_x += 30;
    list->next_y += 30;
    if (list->next_x > 300) {
        list->next_x = 80;
    }
    if (list->next_y > 300) {
        list->next_y = 80;
    }

    AppWindow *window = app_window_create(
        display,
        root,
        screen,
        colormap,
        x,
        y,
        (unsigned int)width,
        (unsigned int)height
    );
    if (window == NULL) {
        return NULL;
    }

    app_window_wrap_child(display, window, child);
    app_window_redraw_title(display, window, screen);
    window_list_add(list, window);

    return window;
}

static void handle_new_window(
    AppWindowList *list,
    Display *display,
    Window root,
    int screen,
    Colormap colormap,
    Window window
) {
    if (window == root) {
        return;
    }

    if (window_list_find(list, window) != NULL) {
        return;
    }

    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, window, &attrs) == 0) {
        return;
    }

    if (attrs.override_redirect) {
        return;
    }

    if (!window_is_top_level(display, root, window)) {
        return;
    }

    int width = attrs.width > 0 ? attrs.width : 900;
    int height = attrs.height > 0 ? attrs.height : 600;
    if (width < 200) {
        width = 900;
    }
    if (height < 120) {
        height = 600;
    }

    wrap_window(list, display, root, screen, colormap, window, width, height);
}

static void handle_destroy(AppWindowList *list, Display *display, Window window) {
    for (size_t i = 0; i < list->count; i++) {
        AppWindow *app = list->items[i];
        if (app->child == window) {
            app_window_destroy(display, app);
            window_list_remove(list, i);
            return;
        }
    }
}

int main(void) {
    Config config;
    config_set_defaults(&config);

    const char *home = getenv("HOME");
    char config_path[512];
    if (home != NULL) {
        snprintf(config_path, sizeof(config_path), "%s/.config/hatDE/config", home);
        config_load(&config, config_path);
    }

    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "hatde: failed to open X display\n");
        return EXIT_FAILURE;
    }

    const int screen = DefaultScreen(display);
    const Window root = RootWindow(display, screen);
    const Colormap colormap = DefaultColormap(display, screen);

    Pixmap background_pixmap = None;
    set_root_background(display, root, colormap, &config, &background_pixmap);

    XSelectInput(display, root, SubstructureNotifyMask);

    const Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
    if (cursor == None) {
        fprintf(stderr, "hatde: failed to create mouse cursor\n");
        XCloseDisplay(display);
        return EXIT_FAILURE;
    }

    AppWindowList windows;
    window_list_init(&windows);

    AppWindow *initial = app_window_create(display, root, screen, colormap, 100, 100, 900, 600);
    if (initial != NULL) {
        app_window_spawn_in_frame(display, initial, config.window_cmd);
        app_window_redraw_title(display, initial, screen);
        window_list_add(&windows, initial);
    }

    XDefineCursor(display, root, cursor);
    XFlush(display);

    signal(SIGINT, stop_running);
    signal(SIGTERM, stop_running);

    while (keep_running) {
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);

            if (event.type == MapNotify) {
                handle_new_window(&windows, display, root, screen, colormap, event.xmap.window);
            } else if (event.type == DestroyNotify) {
                handle_destroy(&windows, display, event.xdestroywindow.window);
            } else if (event.type == PropertyNotify) {
                AppWindow *app = window_list_find(&windows, event.xproperty.window);
                if (app != NULL && app->child == event.xproperty.window) {
                    app_window_set_title_from_child(display, app);
                    app_window_redraw_title(display, app, screen);
                }
            }

            for (size_t i = 0; i < windows.count; i++) {
                app_window_handle_event(windows.items[i], display, screen, &event);
            }
        }

        usleep(10000);
    }

    for (size_t i = 0; i < windows.count; i++) {
        app_window_destroy(display, windows.items[i]);
    }

    free(windows.items);
    if (background_pixmap != None) {
        XFreePixmap(display, background_pixmap);
    }
    XFreeCursor(display, cursor);
    XCloseDisplay(display);
    return EXIT_SUCCESS;
}
