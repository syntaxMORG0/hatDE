#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/xpm.h>
#include <X11/Xft/Xft.h>

#include "app_window.h"
#include "config.h"
#include "launcher.h"

static volatile sig_atomic_t keep_running = 1;

#define XBackgroundColor "#00a2ff"

typedef struct {
    Window window;
    GC gc;
    int active;
    char message[256];
} WarningPopup;

static int parse_color_components(Display *display, Colormap colormap, const char *value, XColor *color) {
    if (value == NULL || value[0] == '\0') {
        return 0;
    }

    if (XParseColor(display, colormap, value, color) == 0) {
        return 0;
    }

    return 1;
}

static void draw_gradient_background(
    Display *display,
    Window root,
    Colormap colormap,
    const char *start_color,
    const char *end_color,
    Pixmap *background_pixmap
) {
    int screen = DefaultScreen(display);
    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);

    XColor start = {0};
    XColor end = {0};
    if (!parse_color_components(display, colormap, start_color, &start) ||
        !parse_color_components(display, colormap, end_color, &end)) {
        return;
    }

    Pixmap pixmap = XCreatePixmap(display, root, (unsigned int)width, (unsigned int)height, DefaultDepth(display, screen));
    GC gc = XCreateGC(display, pixmap, 0, NULL);

    for (int y = 0; y < height; y++) {
        double t = (height <= 1) ? 0.0 : (double)y / (double)(height - 1);
        XColor line = {0};
        line.red = (unsigned short)(start.red + (end.red - start.red) * t);
        line.green = (unsigned short)(start.green + (end.green - start.green) * t);
        line.blue = (unsigned short)(start.blue + (end.blue - start.blue) * t);
        if (XAllocColor(display, colormap, &line) != 0) {
            XSetForeground(display, gc, line.pixel);
            XDrawLine(display, pixmap, gc, 0, y, width, y);
        }
    }

    XFreeGC(display, gc);
    XSetWindowBackgroundPixmap(display, root, pixmap);
    if (background_pixmap != NULL) {
        *background_pixmap = pixmap;
    }
}

static void set_root_background(
    Display *display,
    Window root,
    Colormap colormap,
    const Config *config,
    Pixmap *background_pixmap
) {
    const char *bg_color = XBackgroundColor;
    if (config != NULL && config->background_color[0] != '\0') {
        bg_color = config->background_color;
    }

    XColor background = {0};
    if (XParseColor(display, colormap, bg_color, &background) != 0) {
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
    } else if (config != NULL && strcmp(config->background_mode, "gradient") == 0) {
        draw_gradient_background(
            display,
            root,
            colormap,
            config->gradient_start,
            config->gradient_end,
            background_pixmap
        );
    }

    XClearWindow(display, root);
}

static void stop_running(int signo) {
    (void)signo;
    keep_running = 0;
}

static void warning_activate(WarningPopup *popup) {
    if (popup == NULL) {
        return;
    }
    popup->active = 1;
}

static int font_path_needs_file(const char *font_path) {
    if (font_path == NULL || font_path[0] == '\0') {
        return 0;
    }

    const char *ext = strrchr(font_path, '.');
    if (ext == NULL) {
        return 0;
    }

    return strcmp(ext, ".ttf") == 0 || strcmp(ext, ".otf") == 0;
}

static int font_can_load(Display *display, int screen, const char *font_path) {
    if (font_path == NULL || font_path[0] == '\0') {
        return 0;
    }

    char spec[256];
    if (font_path_needs_file(font_path) && strstr(font_path, "/") != NULL) {
        snprintf(spec, sizeof(spec), "file=%s:size=12", font_path);
    } else {
        snprintf(spec, sizeof(spec), "%s", font_path);
    }

    XftFont *font = XftFontOpenName(display, screen, spec);
    if (font == NULL) {
        return 0;
    }

    XftFontClose(display, font);
    return 1;
}

static int font_path_missing(const char *font_path) {
    if (font_path == NULL || font_path[0] == '\0') {
        return 1;
    }

    const char *ext = strrchr(font_path, '.');
    if (ext == NULL) {
        return 0;
    }

    if (strcmp(ext, ".ttf") != 0 && strcmp(ext, ".otf") != 0) {
        return 0;
    }

    return access(font_path, R_OK) != 0;
}

static void warning_draw(Display *display, WarningPopup *popup, int screen) {
    if (popup == NULL || popup->active == 0) {
        return;
    }

    XClearWindow(display, popup->window);
    if (popup->gc == NULL) {
        return;
    }

    XSetForeground(display, popup->gc, BlackPixel(display, screen));
    XDrawString(display, popup->window, popup->gc, 12, 22, popup->message, (int)strlen(popup->message));
    XDrawString(display, popup->window, popup->gc, 12, 44, "Click to dismiss", 16);
}

static WarningPopup warning_create(
    Display *display,
    Window root,
    int screen,
    const char *message
) {
    WarningPopup popup = {0};
    if (message == NULL || message[0] == '\0') {
        return popup;
    }

    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.override_redirect = True;

    snprintf(popup.message, sizeof(popup.message), "%s", message);
    popup.window = XCreateWindow(
        display,
        root,
        60,
        60,
        420,
        70,
        1,
        CopyFromParent,
        InputOutput,
        CopyFromParent,
        CWOverrideRedirect,
        &attrs
    );
    XSetWindowBackground(display, popup.window, WhitePixel(display, screen));
    popup.gc = XCreateGC(display, popup.window, 0, NULL);
    popup.active = 1;
    XSelectInput(display, popup.window, ExposureMask | ButtonPressMask);
    XMapWindow(display, popup.window);
    XRaiseWindow(display, popup.window);
    return popup;
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
    const char *title_font,
    const char *title_bg,
    const char *title_fg,
    int title_height,
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
        title_font,
        title_bg,
        title_fg,
        title_height,
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
    const char *title_font,
    const char *title_bg,
    const char *title_fg,
    int title_height,
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

    wrap_window(
        list,
        display,
        root,
        screen,
        colormap,
        title_font,
        title_bg,
        title_fg,
        title_height,
        window,
        width,
        height
    );
}

static void handle_destroy(AppWindowList *list, Display *display, Window window) {
    for (size_t i = 0; i < list->count; i++) {
        AppWindow *app = list->items[i];
        if (app->frame == window || app->title == window ||
            app->content == window || app->child == window) {
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
        if (!config_load(&config, config_path)) {
            if (config_write_default(&config, config_path)) {
                config_load(&config, config_path);
            }
        }
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

    Launcher launcher = launcher_create(display, root, screen, colormap, &config);

    XDefineCursor(display, root, cursor);
    XFlush(display);

    signal(SIGINT, stop_running);
    signal(SIGTERM, stop_running);

    WarningPopup warning = {0};
    if (font_path_needs_file(config.font_path) && font_path_missing(config.font_path)) {
        char message[256];
        snprintf(message, sizeof(message), "Font file missing: %s", config.font_path);
        warning = warning_create(display, root, screen, message);
        warning_activate(&warning);
    } else if (!font_can_load(display, screen, config.font_path)) {
        char message[256];
        snprintf(message, sizeof(message), "Font load failed: %s", config.font_path);
        warning = warning_create(display, root, screen, message);
        warning_activate(&warning);
    }

    while (keep_running) {
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);

            if (event.type == MapNotify) {
                handle_new_window(
                    &windows,
                    display,
                    root,
                    screen,
                    colormap,
                    config.font_path,
                    config.title_bg,
                    config.title_fg,
                    config.title_height,
                    event.xmap.window
                );
            } else if (event.type == DestroyNotify) {
                handle_destroy(&windows, display, event.xdestroywindow.window);
            } else if (event.type == PropertyNotify) {
                AppWindow *app = window_list_find(&windows, event.xproperty.window);
                if (app != NULL && app->child == event.xproperty.window) {
                    app_window_set_title_from_child(display, app);
                    app_window_redraw_title(display, app, screen);
                }
            }

            if (launcher_handle_event(display, &launcher, &event)) {
                continue;
            }

            if (warning.active && event.xany.window == warning.window) {
                if (event.type == Expose) {
                    warning_draw(display, &warning, screen);
                } else if (event.type == ButtonPress) {
                    XDestroyWindow(display, warning.window);
                    if (warning.gc != NULL) {
                        XFreeGC(display, warning.gc);
                    }
                    warning.active = 0;
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
    launcher_destroy(display, &launcher);
    if (warning.active) {
        if (warning.window != None) {
            XDestroyWindow(display, warning.window);
        }
        if (warning.gc != NULL) {
            XFreeGC(display, warning.gc);
        }
        warning.active = 0;
    }
    if (background_pixmap != None) {
        XFreePixmap(display, background_pixmap);
    }
    XFreeCursor(display, cursor);
    XCloseDisplay(display);
    return EXIT_SUCCESS;
}
