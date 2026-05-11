# hatDE Config

Config file location:

```
~/.config/hatDE/config
```

If the config file is missing, hatDE writes a default config on startup.

## Example

```ini
WINDOW: "/usr/bin/xterm"
CURSOR: "~/hatDE/src/assets/cursor.svg"
FONT "~/hatDE/src/assets/JetBrainsMonoNerdFont-Regular.ttf"

# desktop
BACKGROUND: "gradient" # color | gradient | image
BACKGROUND_COLOR: "#00a2ff"
GRADIENT_START: "#00a2ff"
GRADIENT_END: "#001933"
BIMAGE: "/path/to/wallpaper.xpm"

# title bar
TITLE_BG: "#2b2f36"
TITLE_FG: "#f5f5f5"
TITLE_HEIGHT: 28

# initial window size
WINDOW_WIDTH: 900
WINDOW_HEIGHT: 600

# launcher
LAUNCHER_ENABLED: 1
LAUNCHER_WIDTH: 360
LAUNCHER_HEIGHT: 32
LAUNCHER_MARGIN: 12
LAUNCHER_BG: "#1b1f26"
LAUNCHER_FG: "#f5f5f5"
```

## Desktop Background

- `BACKGROUND`: one of `color`, `gradient`, `image`.
- `BACKGROUND_COLOR`: solid color used with `color`.
- `GRADIENT_START`/`GRADIENT_END`: gradient colors (top to bottom) used with `gradient`.
- `BIMAGE`: wallpaper path used with `image` (XPM only for now).

## Fonts

- `FONT`: accepts a fontconfig name (example: `JetBrains Mono Nerd Font:size=12`) or a direct file path to `.ttf`/`.otf`.
- If a file path is provided and missing, hatDE shows a warning popup.

## Title Bar

- `TITLE_BG`: title bar background color.
- `TITLE_FG`: title bar text and close button color.
- `TITLE_HEIGHT`: height of the title bar in pixels.

## Initial Window Size

- `WINDOW_WIDTH`/`WINDOW_HEIGHT`: default size for newly wrapped windows.

## Launcher

- `LAUNCHER_ENABLED`: set to `1` to show the launcher, `0` to hide it.
- `LAUNCHER_WIDTH`/`LAUNCHER_HEIGHT`: launcher size.
- `LAUNCHER_MARGIN`: distance from screen edges.
- `LAUNCHER_BG`/`LAUNCHER_FG`: launcher colors.

## Notes

- The launcher runs commands via `/bin/sh -c`.
- Apps launched inside the session (for example `firefox`) are reparented into frames when they map on X11.
