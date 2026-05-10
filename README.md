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

## Safe testing in QEMU (recommended)

You can test everything in a VM so host changes stay isolated.

Install host tools (Debian/Ubuntu):

```bash
sudo apt install qemu-system-x86 qemu-utils
```

Create and install a VM (first time):

```bash
make qemu-install ISO=/absolute/path/to/linux-installer.iso
```

Boot the installed VM later:

```bash
make qemu-run
```

Use an already downloaded `.qcow2` image:

```bash
make qemu-run-qcow QCOW_IMAGE=/absolute/path/to/downloaded-image.qcow2
```

This boots your qcow image directly (no conversion needed).

To import it into `build/qemu/hatde.qcow2` instead (requires `qemu-img`):

```bash
make qemu-import-qcow QCOW_IMAGE=/absolute/path/to/downloaded-image.qcow2
```

Or boot a live ISO directly:

```bash
make qemu-run-live ISO=/absolute/path/to/live-linux.iso
```

### Notes

- VM disk path: `build/qemu/hatde.qcow2`
- Mouse is configured as a USB tablet for reliable pointer behavior in QEMU
- Project folder is shared into the VM via 9p (`mount_tag=hostshare`)
- Host `localhost:9090` is forwarded to guest `:9090` (Cockpit web console)
- If host 9090 is busy, use another port: `make qemu-run VM_WEB_PORT=19090` and open `http://localhost:19090`
- Inside the guest, install build deps (`build-essential`, `libx11-dev`, `pkg-config`), then build and run `hatde`
