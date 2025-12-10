# Makefile for RichEditor - Win32 Text Editor with RichEdit control
# Supports MXE cross-compilation and native MinGW-w64
#
# Usage:
#   make                                    - Build debug version (default)
#   make release                            - Build release version (optimized)
#   make CROSS=x86_64-w64-mingw32.static-   - Override compiler prefix
#   make clean                              - Remove build artifacts
#   make rebuild                            - Clean and build
#
# Build types:
#   debug   - Debug symbols, no optimization, for development
#   release - Size-optimized, stripped, for distribution

# Default cross-compiler prefix (can be overridden)
CROSS ?= x86_64-w64-mingw32.static-

# Compiler and tools
CXX = $(CROSS)g++
WINDRES = $(CROSS)windres
STRIP = $(CROSS)strip

# Project files
TARGET = RichEditor.exe
SRC = src/main.cpp
RC = src/resource.rc
OBJ = main.o resource.o

# Common flags for all builds
COMMON_CFLAGS = -std=c++11 -DUNICODE -D_UNICODE
COMMON_LDFLAGS = -mwindows -municode
LIBS = -lcomctl32 -lcomdlg32 -lole32 -loleaut32 -lshell32

# Debug build flags
# -Og : optimize for debugging (keeps code structure)
# -g  : include debug symbols
# -Wall -Wextra : enable useful warnings
CFLAGS_DEBUG = $(COMMON_CFLAGS) -Og -g -Wall -Wextra
LDFLAGS_DEBUG = $(COMMON_LDFLAGS)

# Release build flags (size optimized)
# -Os : optimize for size
# -s  : strip symbols during linking
# -ffunction-sections -fdata-sections : enable dead code elimination
# -fno-exceptions -fno-rtti : disable C++ features we don't use
# -static : static linking for standalone executable
CFLAGS_RELEASE = $(COMMON_CFLAGS) -Os -ffunction-sections -fdata-sections \
                 -fno-exceptions -fno-rtti -Wall -Wextra \
                 -static -static-libgcc -static-libstdc++
LDFLAGS_RELEASE = $(COMMON_LDFLAGS) -s -Wl,--gc-sections \
                  -static -static-libgcc -static-libstdc++

# Default target (debug build)
.DEFAULT_GOAL := debug

# Build rules
all: debug

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: LDFLAGS = $(LDFLAGS_DEBUG)
debug: $(TARGET)
	@echo ""
	@echo "========================================="
	@echo "Debug build complete: $(TARGET)"
	@echo "Universal build with English and Czech"
	@echo "Size: $$(stat -c%s $(TARGET) 2>/dev/null || stat -f%z $(TARGET) 2>/dev/null || echo 'unknown') bytes"
	@echo "========================================="
	@echo ""

release: CFLAGS = $(CFLAGS_RELEASE)
release: LDFLAGS = $(LDFLAGS_RELEASE)
release: clean $(TARGET)
	@echo ""
	@echo "========================================="
	@echo "Release build complete: $(TARGET)"
	@echo "Universal build with English and Czech"
	@echo "Optimized for size, symbols stripped"
	@echo "Size: $$(stat -c%s $(TARGET) 2>/dev/null || stat -f%z $(TARGET) 2>/dev/null || echo 'unknown') bytes"
	@echo "========================================="
	@echo ""

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) $(OBJ) $(LIBS) -o $(TARGET)

main.o: $(SRC) src/resource.h
	$(CXX) $(CFLAGS) -c $(SRC) -o main.o

resource.o: $(RC) src/resource.h
	$(WINDRES) $(RC) -o resource.o

clean:
	rm -f $(OBJ) $(TARGET)
	@echo "Cleaned build artifacts"

rebuild: clean all

# Help target
help:
	@echo "RichEditor Build System"
	@echo "======================="
	@echo ""
	@echo "Targets:"
	@echo "  make                    - Build debug version (default)"
	@echo "  make debug              - Build debug version with symbols"
	@echo "  make release            - Build release version (optimized for size)"
	@echo "  make clean              - Remove build artifacts"
	@echo "  make rebuild            - Clean and rebuild debug version"
	@echo "  make help               - Show this help"
	@echo ""
	@echo "Cross-compilation:"
	@echo "  make CROSS=x86_64-w64-mingw32.static-   - 64-bit MXE static (default)"
	@echo "  make CROSS=i686-w64-mingw32.static-     - 32-bit MXE static"
	@echo "  make CROSS=x86_64-w64-mingw32-          - 64-bit native MinGW-w64"
	@echo ""
	@echo "Build types:"
	@echo "  debug   - Debug symbols, minimal optimization (-Og -g)"
	@echo "  release - Size optimized, stripped (-Os -s, ~755KB)"
	@echo ""
	@echo "The executable contains both English and Czech resources."
	@echo "Windows automatically selects the language based on system settings."
	@echo ""
	@echo "Examples:"
	@echo "  make                                    # Debug build"
	@echo "  make release                            # Release build"
	@echo "  make release CROSS=i686-w64-mingw32.static-  # 32-bit release"
	@echo ""

.PHONY: all debug release clean rebuild help
