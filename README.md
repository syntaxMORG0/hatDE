# hatDE

A minimal X11 desktop.

# INCOMPLETE!!

## Build

Install X11/Xpm/Xft development headers first.

Debian/Ubuntu (APT):

```bash
sudo apt install libx11-dev libxpm-dev libxft-dev libfontconfig-dev pkg-config
```

Arch (Pacman):

```bash
sudo pacman -S --needed libx11 libxpm libxft fontconfig pkgconf
```

Then build:

```bash
make
```

See all available make targets:

```bash
make help
```

## Run (inside an X11 session)

```bash
echo "exec /path/to/hatde" >> "~/.xinitrc"
startx
```

## Config

hatDE reads `~/.config/hatDE/config` (based on `config_example`).

Example:

```ini
WINDOW: "/usr/bin/xterm"
CURSOR: "~/hatDE/src/assets/cursor.svg"
FONT "~/hatDE/src/assets/JetBrainsMonoNerdFont-Regular.ttf"

# desktop
BACKGROUND: "image" # color | gradient | image
BIMAGE: "/path/to/wallpaper.xpm"
```

Notes:
- `BACKGROUND: "image"` uses XPM files for now.
- Apps launched from the terminal (for example `firefox`) are reparented into frames when they map on X11.
