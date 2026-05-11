#include "launcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/keysym.h>

static void launcher_make_font_spec(const char *font_name, char *buffer, size_t size) {
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

static unsigned long launcher_parse_color(
    Display *display,
    Colormap colormap,
    const char *name,
    unsigned long fallback
) {
    if (name == NULL || name[0] == '\0') {
        return fallback;
    }

    XColor color = {0};
    if (XParseColor(display, colormap, name, &color) == 0) {
        return fallback;
    }

    if (XAllocColor(display, colormap, &color) == 0) {
        return fallback;
    }

    return color.pixel;
}

Launcher launcher_create(
    Display *display,
    Window root,
    int screen,
    Colormap colormap,
    const Config *config
) {
    Launcher launcher = {0};
    if (config == NULL || config->launcher_enabled == 0) {
        return launcher;
    }

    int screen_width = DisplayWidth(display, screen);
    int screen_height = DisplayHeight(display, screen);

    int height = (config->launcher_height > 0) ? config->launcher_height : 32;
    int width = (config->launcher_width > 0) ? config->launcher_width : 360;
    int margin = (config->launcher_margin >= 0) ? config->launcher_margin : 12;

    int x = margin;
    int y = screen_height - height - margin;
    if (y < margin) {
        y = margin;
    }

    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.override_redirect = True;

    launcher.window = XCreateWindow(
        display,
        root,
        x,
        y,
        (unsigned int)width,
        (unsigned int)height,
        1,
        CopyFromParent,
        InputOutput,
        CopyFromParent,
        CWOverrideRedirect,
        &attrs
    );

    unsigned long bg_pixel = launcher_parse_color(
        display,
        colormap,
        config->launcher_bg,
        BlackPixel(display, screen)
    );
    XSetWindowBackground(display, launcher.window, bg_pixel);

    launcher.gc = XCreateGC(display, launcher.window, 0, NULL);
    launcher.fg_pixel = launcher_parse_color(
        display,
        colormap,
        config->launcher_fg,
        WhitePixel(display, screen)
    );

    char font_spec[256];
    launcher_make_font_spec(config->font_path, font_spec, sizeof(font_spec));
    launcher.draw = XftDrawCreate(
        display,
        launcher.window,
        DefaultVisual(display, screen),
        colormap
    );
    if (launcher.draw != NULL) {
        launcher.font = XftFontOpenName(display, screen, font_spec);
        if (launcher.font == NULL) {
            XftDrawDestroy(launcher.draw);
            launcher.draw = NULL;
        }
    }

    if (launcher.draw != NULL) {
        XftColorAllocName(
            display,
            DefaultVisual(display, screen),
            colormap,
            config->launcher_fg,
            &launcher.color
        );
    }

    XSelectInput(display, launcher.window, ExposureMask | ButtonPressMask | KeyPressMask);
    XMapWindow(display, launcher.window);
    XRaiseWindow(display, launcher.window);
    XSetInputFocus(display, launcher.window, RevertToParent, CurrentTime);
    launcher.active = 1;

    return launcher;
}

void launcher_destroy(Display *display, Launcher *launcher) {
    if (launcher == NULL || launcher->active == 0) {
        return;
    }

    if (launcher->gc != NULL) {
        XFreeGC(display, launcher->gc);
    }

    if (launcher->draw != NULL) {
        XftDrawDestroy(launcher->draw);
    }

    if (launcher->font != NULL) {
        XftFontClose(display, launcher->font);
    }

    if (launcher->color.pixel != 0) {
        XftColorFree(
            display,
            DefaultVisual(display, DefaultScreen(display)),
            DefaultColormap(display, DefaultScreen(display)),
            &launcher->color
        );
    }

    if (launcher->window != None) {
        XDestroyWindow(display, launcher->window);
    }

    launcher->active = 0;
}

void launcher_draw(Display *display, Launcher *launcher, int screen) {
    if (launcher == NULL || launcher->active == 0) {
        return;
    }

    XClearWindow(display, launcher->window);

    if (launcher->draw != NULL && launcher->font != NULL) {
        XftDrawStringUtf8(
            launcher->draw,
            &launcher->color,
            launcher->font,
            10,
            20,
            (const FcChar8 *)launcher->input,
            (int)strlen(launcher->input)
        );
        return;
    }

    if (launcher->gc == NULL) {
        return;
    }

    XSetForeground(display, launcher->gc, launcher->fg_pixel);
    XDrawString(
        display,
        launcher->window,
        launcher->gc,
        10,
        20,
        launcher->input,
        (int)strlen(launcher->input)
    );
}

static void launcher_execute_command(const char *command) {
    if (command == NULL || command[0] == '\0') {
        return;
    }

    pid_t child = fork();
    if (child == 0) {
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(EXIT_FAILURE);
    }
}

int launcher_handle_event(Display *display, Launcher *launcher, XEvent *event) {
    if (launcher == NULL || launcher->active == 0 || event == NULL) {
        return 0;
    }

    if (event->xany.window != launcher->window) {
        return 0;
    }

    if (event->type == Expose && event->xexpose.count == 0) {
        launcher_draw(display, launcher, DefaultScreen(display));
        return 1;
    }

    if (event->type == ButtonPress) {
        XSetInputFocus(display, launcher->window, RevertToParent, CurrentTime);
        return 1;
    }

    if (event->type == KeyPress) {
        char buffer[64];
        KeySym keysym;
        int len = XLookupString(&event->xkey, buffer, sizeof(buffer), &keysym, NULL);

        if (keysym == XK_Return) {
            launcher_execute_command(launcher->input);
            launcher->input[0] = '\0';
            launcher_draw(display, launcher, DefaultScreen(display));
            return 1;
        }

        if (keysym == XK_Escape) {
            launcher->input[0] = '\0';
            launcher_draw(display, launcher, DefaultScreen(display));
            return 1;
        }

        if (keysym == XK_BackSpace) {
            size_t current = strlen(launcher->input);
            if (current > 0) {
                launcher->input[current - 1] = '\0';
                launcher_draw(display, launcher, DefaultScreen(display));
            }
            return 1;
        }

        if (len > 0) {
            size_t current = strlen(launcher->input);
            if (current + (size_t)len < sizeof(launcher->input)) {
                memcpy(launcher->input + current, buffer, (size_t)len);
                launcher->input[current + (size_t)len] = '\0';
                launcher_draw(display, launcher, DefaultScreen(display));
            }
            return 1;
        }
    }

    return 0;
}
