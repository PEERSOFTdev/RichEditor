# Makefile for RichEditor - Win32 Text Editor with RichEdit control
# Supports MXE cross-compilation and native MinGW-w64
#
# Usage:
#   make                                    - Build debug version (default)
#   make strip                              - Strip debug symbols for distribution
#   make CROSS=x86_64-w64-mingw32.static-   - Override compiler prefix
#   make clean                              - Remove build artifacts
#   make rebuild                            - Clean and build
#
# Build types:
#   debug   - Debug symbols with -O2 optimization (~875KB)
#   strip   - Stripped debug build for distribution (~275KB)

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

# Compiler flags
# -O2 : standard optimization level (balanced performance/size)
# -g  : include debug symbols
# -Wall -Wextra : enable useful warnings
# -static : static linking for standalone executable
# -ffunction-sections/-fdata-sections + -Wl,--gc-sections : dead code elimination
# -flto : link-time optimization
CFLAGS = -std=c++11 -DUNICODE -D_UNICODE -O2 -g -Wall -Wextra \
         -static -static-libgcc -static-libstdc++ \
         -ffunction-sections -fdata-sections -Wl,--gc-sections -flto
LDFLAGS = -mwindows -municode -static -static-libgcc -static-libstdc++
LIBS = -lcomctl32 -lcomdlg32 -lole32 -loleaut32 -lshell32

# Default target (debug build)
.DEFAULT_GOAL := debug

# Build rules
all: $(TARGET)
	@echo ""
	@echo "========================================="
	@echo "Build complete: $(TARGET)"
	@echo "Universal build with English and Czech"
	@echo "Size: $$(stat -c%s $(TARGET) 2>/dev/null || stat -f%z $(TARGET) 2>/dev/null || echo 'unknown') bytes"
	@echo "========================================="
	@echo ""

debug: all

strip: all
	@echo "Stripping debug symbols from $(TARGET)..."
	$(STRIP) $(TARGET)
	@echo ""
	@echo "========================================="
	@echo "Stripped build complete: $(TARGET)"
	@echo "Optimized for distribution"
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
	@echo "  make                    - Build with debug symbols (default)"
	@echo "  make strip              - Strip debug symbols for distribution"
	@echo "  make clean              - Remove build artifacts"
	@echo "  make rebuild            - Clean and rebuild"
	@echo "  make help               - Show this help"
	@echo ""
	@echo "Cross-compilation:"
	@echo "  make CROSS=x86_64-w64-mingw32.static-   - 64-bit MXE static (default)"
	@echo "  make CROSS=i686-w64-mingw32.static-     - 32-bit MXE static"
	@echo "  make CROSS=x86_64-w64-mingw32-          - 64-bit native MinGW-w64"
	@echo ""
	@echo "Build sizes:"
	@echo "  make        - Debug build with symbols (~875KB)"
	@echo "  make strip  - Stripped for distribution (~275KB)"
	@echo ""
	@echo "The executable contains both English and Czech resources."
	@echo "Windows automatically selects the language based on system settings."
	@echo ""
	@echo "Examples:"
	@echo "  make                                         # Build with symbols"
	@echo "  make strip                                   # Strip for distribution"
	@echo "  make strip CROSS=i686-w64-mingw32.static-    # 32-bit stripped build"
	@echo ""

.PHONY: all debug strip clean rebuild help
