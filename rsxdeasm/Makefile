# ==============================================================================
# PS3 OpenGraphics Shader Decompiler / Disassembler (rsxdeasm)
# ==============================================================================

CXX      ?= g++
CXXFLAGS ?= -O3 -Wall -Wextra -Wno-pedantic -std=c++20

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin
DESTDIR  ?=

TARGET   = rsxdeasm

SRCS     = src/rsx_disasm.cpp
OBJS     = $(SRCS:.cpp=.o)

.PHONY: all clean install uninstall help

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "  [LD]  $@"
	@$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	@echo "  [CXX] $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

install: all
	@echo "Installing $(TARGET) to $(DESTDIR)$(BINDIR)..."
	@install -d $(DESTDIR)$(BINDIR)
	@install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	@echo "Uninstalling $(TARGET) from $(DESTDIR)$(BINDIR)..."
	@gio trash $(DESTDIR)$(BINDIR)/$(TARGET) 2>/dev/null || true

clean:
	@echo "Cleaning rsxdeasm build artifacts..."
	@gio trash $(OBJS) $(TARGET) 2>/dev/null || true

help:
	@echo "rsxdeasm Makefile Targets:"
	@echo "  all       - Build $(TARGET) binary"
	@echo "  install   - Install binary to $(DESTDIR)$(BINDIR)"
	@echo "  uninstall - Remove installed binary"
	@echo "  clean     - Remove compiled object files and binary"
	@echo "  help      - Show this help message"
