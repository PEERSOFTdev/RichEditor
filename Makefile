# Makefile for RichEditor - Win32 Text Editor with RichEdit control
# Supports both MXE cross-compilation and native MinGW-w64

# Compiler and tools
CC = x86_64-w64-mingw32-g++
WINDRES = x86_64-w64-mingw32-windres

# Compiler flags
CFLAGS = -O2 -std=c++11 -DUNICODE -D_UNICODE -Wall -Wextra
LDFLAGS = -mwindows -static -static-libgcc -static-libstdc++
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
	@echo "  make          - Build RichEditor.exe"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make rebuild  - Clean and build"
	@echo "  make help     - Show this help"
	@echo ""
	@echo "Requirements:"
	@echo "  - MinGW-w64 (x86_64-w64-mingw32-g++)"
	@echo "  - Windows 7 or later (target OS)"
	@echo ""

.PHONY: all clean rebuild help
