# ==============================================================================
# PS3 OpenGraphics Shader Decompiler / Disassembler (rsxdeasm)
# ==============================================================================

CC       ?= gcc
CFLAGS   ?= -O3 -Wall -Wextra -Wno-pedantic -std=c11

CXX      ?= g++
CXXFLAGS ?= -O3 -Wall -Wextra -Wno-pedantic -std=c++20

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin
DESTDIR  ?=

TARGET       = rsxdeasm
TARGET_CPP   = rsxdeasm-cpp

C_SRCS       = src/rsx_disasm.c
C_OBJS       = $(C_SRCS:.c=.o)

CPP_SRCS     = src/rsx_disasm.cpp
CPP_OBJS     = $(CPP_SRCS:.cpp=.cpp.o)

.PHONY: all clean install uninstall help cpp

all: $(TARGET)

$(TARGET): $(C_OBJS)
	@echo "  [LD]  $@"
	@$(CC) $(CFLAGS) -o $@ $(C_OBJS)

%.o: %.c
	@echo "  [CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

cpp: $(TARGET_CPP)

$(TARGET_CPP): $(CPP_OBJS)
	@echo "  [LD-CXX] $@"
	@$(CXX) $(CXXFLAGS) -o $@ $(CPP_OBJS)

%.cpp.o: %.cpp
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
	@gio trash $(C_OBJS) $(CPP_OBJS) $(TARGET) $(TARGET_CPP) 2>/dev/null || true

help:
	@echo "rsxdeasm Makefile Targets:"
	@echo "  all       - Build standard C $(TARGET) binary (default)"
	@echo "  cpp       - Build C++ $(TARGET_CPP) binary"
	@echo "  install   - Install binary to $(DESTDIR)$(BINDIR)"
	@echo "  uninstall - Remove installed binary"
	@echo "  clean     - Remove compiled object files and binaries"
	@echo "  help      - Show this help message"
