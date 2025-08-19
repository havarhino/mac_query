# Compiler to use
CC = gcc

# Output executable name
TARGET = get_mac

# Library name
LIB = libmacutils.a

# Directories
LIBDIR = ./lib
INCDIR = ./include

# Source files
LIB_SRC = mac_utils.c
MAIN_SRC = main.c

# Object file for library
LIB_OBJ = $(LIB_SRC:.c=.o)

# Default flags
CFLAGS = -Wall -O2 -I$(INCDIR)
LDFLAGS =

# Platform-specific settings
UNAME_S := $(shell uname -s)

# Phony targets
.PHONY: all install clean build_and_install

# Linux/Raspberry Pi
ifeq ($(UNAME_S),Linux)
    LDFLAGS +=
endif

# macOS
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -framework IOKit -framework CoreFoundation
endif

# Windows (MinGW/MSYS2 or similar)
ifeq ($(OS),Windows_NT)
    LDFLAGS += -liphlpapi
endif

# Default target
all: build_and_install $(TARGET)

# Install target: create directories, build library, copy header
build_and_install: install $(TARGET)

# Build the library
$(LIBDIR)/$(LIB): $(LIB_OBJ)
	@mkdir -p $(LIBDIR)
	ar rcs $@ $^

# Build object file for library
$(LIB_OBJ): $(LIB_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

# Install target: create directories, build library, copy header
install: $(LIBDIR)/$(LIB)
	@mkdir -p $(INCDIR)
	cp mac_utils.h $(INCDIR)/

# Build the executable
$(TARGET): $(MAIN_SRC) $(LIBDIR)/$(LIB)
	$(CC) $(CFLAGS) $< -o $@ -L$(LIBDIR) -lmacutils $(LDFLAGS)

# Clean up
clean:
	rm -f $(TARGET) $(LIB_OBJ) $(LIBDIR)/$(LIB)
	rm -rf $(LIBDIR) $(INCDIR)

