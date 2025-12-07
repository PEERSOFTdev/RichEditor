# Makefile for RichEditor - Win32 Text Editor with RichEdit control
# Supports MXE cross-compilation and native MinGW-w64
#
# Usage with MXE:
#   make CROSS=i686-w64-mingw32.static-
#   make CROSS=x86_64-w64-mingw32.static-
#
# Usage with native MinGW-w64:
#   make CROSS=i686-w64-mingw32-
#   make CROSS=x86_64-w64-mingw32-

# Default cross-compiler prefix (can be overridden)
CROSS ?= x86_64-w64-mingw32-

# Compiler and tools
CC = $(CROSS)g++
WINDRES = $(CROSS)windres

# Compiler flags
CFLAGS = -O2 -std=c++11 -DUNICODE -D_UNICODE -Wall -Wextra
LDFLAGS = -mwindows -municode -static -static-libgcc -static-libstdc++
LIBS = -lcomctl32 -lcomdlg32 -lole32 -loleaut32 -lshell32

# Output
TARGET = RichEditor.exe

# Source files
SRC = src/main.cpp
RC = src/resource.rc
OBJ = main.o resource.o

# Build rules
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $(TARGET)
	@echo ""
	@echo "========================================="
	@echo "Build complete: $(TARGET)"
	@echo "========================================="
	@echo ""

main.o: $(SRC) src/resource.h
	$(CC) $(CFLAGS) -c $(SRC) -o main.o

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
	@echo "  make                                    - Build with default (x86_64-w64-mingw32-)"
	@echo "  make CROSS=i686-w64-mingw32.static-     - Build 32-bit with MXE static"
	@echo "  make CROSS=x86_64-w64-mingw32.static-   - Build 64-bit with MXE static"
	@echo "  make clean                              - Remove build artifacts"
	@echo "  make rebuild                            - Clean and build"
	@echo "  make help                               - Show this help"
	@echo ""
	@echo "Examples:"
	@echo "  MXE 32-bit:    make CROSS=i686-w64-mingw32.static-"
	@echo "  MXE 64-bit:    make CROSS=x86_64-w64-mingw32.static-"
	@echo "  Native MinGW:  make CROSS=x86_64-w64-mingw32-"
	@echo ""

.PHONY: all clean rebuild help
