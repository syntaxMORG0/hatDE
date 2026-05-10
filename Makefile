CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
X11_CFLAGS ?= $(shell pkg-config --cflags x11 2>/dev/null)
X11_LIBS ?= $(shell pkg-config --libs x11 2>/dev/null)
LDLIBS ?=

TARGET := hatde
SRC := src/main.c
OBJ := $(SRC:.c=.o)

QEMU ?= qemu-system-x86_64
QEMU_IMG ?= qemu-img
VM_DIR ?= build/qemu
VM_DISK ?= $(VM_DIR)/hatde.qcow2
VM_DISK_SIZE ?= 16G
VM_MEMORY ?= 2048
VM_CPUS ?= 2
VM_SSH_PORT ?= 2222
ISO ?=
QCOW_IMAGE ?=
SHARE_DIR ?= $(CURDIR)

QEMU_COMMON := \
	-machine q35,accel=kvm:tcg \
	-cpu max \
	-smp $(VM_CPUS) \
	-m $(VM_MEMORY) \
	-device virtio-vga \
	-display gtk \
	-device qemu-xhci \
	-device usb-tablet \
	-device virtio-net-pci,netdev=net0 \
	-netdev user,id=net0,hostfwd=tcp::$(VM_SSH_PORT)-:22 \
	-virtfs local,path=$(SHARE_DIR),mount_tag=hostshare,security_model=none,id=hostshare

.PHONY: all help clean qemu-check-qcow qemu-create-disk qemu-import-qcow qemu-install qemu-run qemu-run-live qemu-run-qcow qemu-help

all: $(TARGET)

help:
	@echo "hatDE make targets:"
	@echo "  make / make all      Build hatde"
	@echo "  make clean           Remove build artifacts"
	@echo "  make qemu-help       Show QEMU-specific help"
	@echo ""
	@echo "QEMU targets:"
	@echo "  make qemu-create-disk"
	@echo "  make qemu-install ISO=/abs/path/to/installer.iso"
	@echo "  make qemu-run"
	@echo "  make qemu-run-qcow QCOW_IMAGE=/abs/path/to/downloaded-image.qcow2"
	@echo "  make qemu-import-qcow QCOW_IMAGE=/abs/path/to/downloaded-image.qcow2"
	@echo "  make qemu-run-live ISO=/abs/path/to/live.iso"

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(X11_CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ)

qemu-create-disk:
	mkdir -p "$(VM_DIR)"
	@if [ ! -f "$(VM_DISK)" ]; then \
		"$(QEMU_IMG)" create -f qcow2 "$(VM_DISK)" "$(VM_DISK_SIZE)"; \
	else \
		echo "Disk already exists: $(VM_DISK)"; \
	fi

qemu-install: qemu-create-disk
	@if [ -z "$(ISO)" ]; then \
		echo "Set ISO=/absolute/path/to/linux-installer.iso"; \
		exit 1; \
	fi
	"$(QEMU)" $(QEMU_COMMON) \
		-drive file="$(VM_DISK)",if=virtio,format=qcow2 \
		-cdrom "$(ISO)" \
		-boot order=d

qemu-run: qemu-create-disk
	"$(QEMU)" $(QEMU_COMMON) \
		-drive file="$(VM_DISK)",if=virtio,format=qcow2 \
		-boot order=c

qemu-check-qcow:
	@if [ -z "$(QCOW_IMAGE)" ]; then \
		echo "Set QCOW_IMAGE=/absolute/path/to/downloaded-image.qcow2"; \
		exit 1; \
	fi
	@if [ ! -f "$(QCOW_IMAGE)" ]; then \
		echo "QCOW image not found: $(QCOW_IMAGE)"; \
		exit 1; \
	fi

qemu-import-qcow:
	@if ! command -v "$(QEMU_IMG)" >/dev/null 2>&1; then \
		echo "Missing $(QEMU_IMG). Install qemu-utils (Debian/Ubuntu) or qemu-img (Fedora)."; \
		exit 1; \
	fi
	@$(MAKE) --no-print-directory qemu-check-qcow QCOW_IMAGE="$(QCOW_IMAGE)"
	mkdir -p "$(VM_DIR)"
	"$(QEMU_IMG)" convert -O qcow2 "$(QCOW_IMAGE)" "$(VM_DISK)"
	@echo "Imported qcow image to $(VM_DISK)"

qemu-run-qcow: qemu-check-qcow
	"$(QEMU)" $(QEMU_COMMON) \
		-drive file="$(QCOW_IMAGE)",if=virtio,format=qcow2 \
		-boot order=c

qemu-run-live:
	@if [ -z "$(ISO)" ]; then \
		echo "Set ISO=/absolute/path/to/live-linux.iso"; \
		exit 1; \
	fi
	"$(QEMU)" $(QEMU_COMMON) \
		-cdrom "$(ISO)" \
		-boot order=d

qemu-help:
	@echo "QEMU workflow:"
	@echo "  make qemu-install ISO=/abs/path/to/installer.iso"
	@echo "  make qemu-run"
	@echo "  make qemu-run-qcow QCOW_IMAGE=/abs/path/to/downloaded-image.qcow2"
	@echo "  make qemu-import-qcow QCOW_IMAGE=/abs/path/to/downloaded-image.qcow2"
	@echo "  make qemu-run-live ISO=/abs/path/to/live.iso"
	@echo "Variables:"
	@echo "  VM_DISK=$(VM_DISK)"
	@echo "  QCOW_IMAGE=$(QCOW_IMAGE)"
	@echo "  VM_DISK_SIZE=$(VM_DISK_SIZE)"
	@echo "  VM_MEMORY=$(VM_MEMORY) MB, VM_CPUS=$(VM_CPUS)"
	@echo "  SHARE_DIR=$(SHARE_DIR) (9p mount tag: hostshare)"
	@echo "  VM_SSH_PORT=$(VM_SSH_PORT)"

ifeq ($(strip $(X11_LIBS)),)
LDLIBS += -lX11
else
LDLIBS += $(X11_LIBS)
endif
