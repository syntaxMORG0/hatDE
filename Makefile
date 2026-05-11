CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
X11_CFLAGS ?= $(shell pkg-config --cflags x11 2>/dev/null)
X11_LIBS ?= $(shell pkg-config --libs x11 2>/dev/null)
XPM_CFLAGS ?= $(shell pkg-config --cflags xpm 2>/dev/null)
XPM_LIBS ?= $(shell pkg-config --libs xpm 2>/dev/null)
XFT_CFLAGS ?= $(shell pkg-config --cflags xft 2>/dev/null)
XFT_LIBS ?= $(shell pkg-config --libs xft 2>/dev/null)
LDLIBS ?=

TARGET := hatde
SRC := src/main.c src/app_window.c src/config.c src/launcher.c
OBJ := $(SRC:.c=.o)

.PHONY: all help

all: $(TARGET)

help:
	@echo "hatDE make targets:"
	@echo "  make / make all      Build hatde"
	@echo "  make clean           Remove build artifacts"

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(X11_CFLAGS) $(XPM_CFLAGS) $(XFT_CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ)

ifeq ($(strip $(X11_LIBS)),)
LDLIBS += -lX11
else
LDLIBS += $(X11_LIBS)
endif

ifeq ($(strip $(XPM_LIBS)),)
LDLIBS += -lXpm
else
LDLIBS += $(XPM_LIBS)
endif

ifeq ($(strip $(XFT_LIBS)),)
LDLIBS += -lXft -lfontconfig
else
LDLIBS += $(XFT_LIBS)
endif
