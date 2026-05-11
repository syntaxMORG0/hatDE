# hatDE

A minimal X11 desktop base intended to run with the picom compositor.

## Current behavior

- Paints the full root window with sky blue `#00a2ff`
- Sets a working left-pointer mouse cursor on the root window

## Build

Install X11 development headers first (example on Debian/Ubuntu):

```bash
sudo apt install libx11-dev pkg-config
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
./hatde
```

Run `picom` separately in the same session.

config`), then build and run `hatde`
