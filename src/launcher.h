#pragma once

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "config.h"

typedef struct Launcher {
    Window window;
    GC gc;
    XftDraw *draw;
    XftFont *font;
    XftColor color;
    unsigned long fg_pixel;
    int active;
    char input[256];
} Launcher;

Launcher launcher_create(
    Display *display,
    Window root,
    int screen,
    Colormap colormap,
    const Config *config
);

void launcher_destroy(Display *display, Launcher *launcher);
void launcher_draw(Display *display, Launcher *launcher, int screen);
int launcher_handle_event(Display *display, Launcher *launcher, XEvent *event);
