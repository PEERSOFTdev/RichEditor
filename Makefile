# Makefile for RichEditor - Win32 Text Editor with RichEdit control
# Supports MXE cross-compilation and native MinGW-w64
#
# Usage:
#   make                                    - Build debug version (default)
#   make strip                              - Strip debug symbols for distribution
#   make dist                               - Build stripped zip for release
#   make CROSS=x86_64-w64-mingw32.static-   - Override compiler prefix
#   make clean                              - Remove build artifacts
#   make rebuild                            - Clean and build
#
# Build types:
#   debug   - Debug symbols with -Os optimization (~875KB)
#   strip   - Stripped debug build for distribution (~275KB)
#   dist    - Stripped build packaged as release zip with docs

# Extract version from resource.rc  (e.g. FILEVERSION 2,9,0,0 -> 2.9.0)
VERSION := $(shell grep -m1 '^ *FILEVERSION' src/resource.rc | \
             sed 's/.*FILEVERSION *//;s/,[0-9]*$$//;s/,/./g;s/ *$$//')
DIST_VARIANT ?= mingw
DIST_NAME = RichEditor-v$(VERSION)-$(DIST_VARIANT)
DIST_DIR = dist/$(DIST_NAME)

# Default cross-compiler prefix (can be overridden)
CROSS ?= x86_64-w64-mingw32-

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
# -Os : optimize for size (keeps most O2 optimizations)
# -g  : include debug symbols (removed by strip target)
# -Wall -Wextra : enable useful warnings
# -static : static linking for standalone executable
# -ffunction-sections/-fdata-sections : place each function/data in its own section
# -flto : link-time optimization (must appear in both CFLAGS and LDFLAGS)
# -fno-exceptions : no C++ exception runtime (app uses no throw/catch; saves ~2 KB)
CFLAGS = -std=c++11 -DUNICODE -D_UNICODE -Os -g -Wall -Wextra \
         -static -static-libgcc -static-libstdc++ \
         -ffunction-sections -fdata-sections -flto -fno-exceptions
# Linker flags
# -Wl,--gc-sections : remove unreferenced sections prepared by -ffunction-sections
# -flto -Os : LTO at size-optimized level (must match CFLAGS -Os)
# -fno-exceptions : must also appear at link time for LTO to honour it
LDFLAGS = -mwindows -municode -static -static-libgcc -static-libstdc++ \
          -Wl,--gc-sections -flto -Os -fno-exceptions
LIBS = -lcomctl32 -lcomdlg32 -lole32 -loleaut32 -lshell32 -lshlwapi -lversion -loleacc

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

# Release zip: stripped exe + docs in a versioned directory
dist: strip
	@echo "Packaging $(DIST_NAME).zip ..."
	@rm -rf $(DIST_DIR) dist/$(DIST_NAME).zip
	@mkdir -p $(DIST_DIR)/docs
	@cp $(TARGET) $(DIST_DIR)/
	@cp LICENSE $(DIST_DIR)/
	@cp README.md $(DIST_DIR)/
	@cp README_CS.md $(DIST_DIR)/
	@cp docs/USER_MANUAL_EN.md $(DIST_DIR)/docs/
	@cp docs/USER_MANUAL_CS.md $(DIST_DIR)/docs/
	@cp docs/CHANGELOG.md $(DIST_DIR)/docs/
	@cd dist && zip -r $(DIST_NAME).zip $(DIST_NAME)/
	@echo ""
	@echo "========================================="
	@echo "Release archive: dist/$(DIST_NAME).zip"
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

distclean: clean
	rm -rf dist/
	@echo "Cleaned dist directory"

rebuild: clean all

# Help target
help:
	@echo "RichEditor Build System"
	@echo "======================="
	@echo ""
	@echo "Targets:"
	@echo "  make                    - Build with debug symbols (default)"
	@echo "  make strip              - Strip debug symbols for distribution"
	@echo "  make dist               - Build release zip with docs"
	@echo "  make clean              - Remove build artifacts"
	@echo "  make distclean          - Remove build artifacts and dist/"
	@echo "  make rebuild            - Clean and rebuild"
	@echo "  make help               - Show this help"
	@echo ""
	@echo "Cross-compilation:"
	@echo "  make CROSS=x86_64-w64-mingw32-          - 64-bit native MinGW-w64 (default)"
	@echo "  make CROSS=x86_64-w64-mingw32.static-   - 64-bit MXE static"
	@echo "  make CROSS=i686-w64-mingw32.static-     - 32-bit MXE static"
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

.PHONY: all debug strip dist clean distclean rebuild help
