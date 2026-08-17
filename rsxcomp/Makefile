# ==============================================================================
# PS3 OpenGraphics Shader Compiler (rsxcomp)
# ==============================================================================

CC       ?= gcc
CFLAGS   ?= -O2 -Wall -Wextra -Iinclude -std=c11

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin
DESTDIR  ?=

TARGET   = rsxcomp

SRCS     = src/main.c \
           src/rsx_nv40_vertprog.c \
           src/rsx_nv40_fragprog.c \
           src/rsx_tgsi_parser.c \
           src/rsx_spirv_parser.c \
           src/rsx_packager.c

OBJS     = $(SRCS:.c=.o)

.PHONY: all clean install uninstall help

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "  [LD]  $@"
	@$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	@echo "  [CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

install: all
	@echo "Installing $(TARGET) to $(DESTDIR)$(BINDIR)..."
	@install -d $(DESTDIR)$(BINDIR)
	@install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	@echo "Uninstalling $(TARGET) from $(DESTDIR)$(BINDIR)..."
	@gio trash $(DESTDIR)$(BINDIR)/$(TARGET) 2>/dev/null || true

clean:
	@echo "Cleaning rsxcomp build artifacts..."
	@gio trash $(OBJS) $(TARGET) 2>/dev/null || true

help:
	@echo "rsxcomp Makefile Targets:"
	@echo "  all       - Build $(TARGET) binary"
	@echo "  install   - Install binary to $(DESTDIR)$(BINDIR)"
	@echo "  uninstall - Remove installed binary"
	@echo "  clean     - Remove compiled object files and binary"
	@echo "  help      - Show this help message"
