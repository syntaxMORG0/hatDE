#pragma once

#include <sys/types.h>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

typedef struct AppWindow {
    Window frame;
    Window title;
    Window content;
    Window child;
    GC title_gc;
    XftDraw *title_draw;
    XftFont *title_font;
    XftColor title_color;
    pid_t child_pid;
    int title_height;
    int dragging;
    int drag_start_x;
    int drag_start_y;
    int win_start_x;
    int win_start_y;
    char title_text[256];
} AppWindow;

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
);

void app_window_destroy(Display *display, AppWindow *window);
void app_window_set_title(AppWindow *window, const char *title);
void app_window_set_title_from_child(Display *display, AppWindow *window);
void app_window_redraw_title(Display *display, AppWindow *window, int screen);
void app_window_spawn_in_frame(Display *display, AppWindow *window, const char *application);
void app_window_wrap_child(Display *display, AppWindow *window, Window child);
int app_window_handle_event(AppWindow *window, Display *display, int screen, XEvent *event);
