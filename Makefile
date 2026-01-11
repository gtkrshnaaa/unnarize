CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O3 -march=native -mtune=native -Iinclude -Icorelib/include -D_POSIX_C_SOURCE=200809L
LDFLAGS = -ldl -lpthread -Wl,-export-dynamic

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
LIST_DIR = z_listing

# Source files
SRCS_MAIN = $(wildcard $(SRC_DIR)/*.c) \
            $(wildcard $(SRC_DIR)/bytecode/*.c) \
            $(wildcard $(SRC_DIR)/runtime/*.c)
SRCS_CORE = $(wildcard corelib/src/*.c)

SRCS = $(SRCS_MAIN) $(SRCS_CORE)

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS_MAIN)) \
       $(patsubst corelib/src/%.c, $(OBJ_DIR)/corelib/%.o, $(SRCS_CORE))

# Target executable
TARGET = $(BIN_DIR)/unnarize

# Plug-ins
PLUGINS_SRCDIR = examples/plugins/src
PLUGINS_BUILDDIR = examples/plugins/build

# Default target
all: compile

# Compile target
compile: $(TARGET)

# Link
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	@echo "Linking $(TARGET)..."
	@$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Build successful!"

# Compile main source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Compile corelib source files
$(OBJ_DIR)/corelib/%.o: corelib/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

$(PLUGINS_BUILDDIR):
	mkdir -p $(PLUGINS_BUILDDIR)

# Clean target
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR) $(LIST_DIR) $(PLUGINS_BUILDDIR)

# Install target (Fresh build)
install: clean compile
	@echo "Installing unnarize requires root privileges..."
	@echo "Use: sudo make install"
	@install -d /usr/local/bin
	@install -m 755 $(TARGET) /usr/local/bin/unnarize
	@echo "Unnarize installed successfully!"

uninstall:
	@echo "Uninstalling unnarize requires root privileges..."
	@rm -f /usr/local/bin/unnarize
	@echo "Unnarize uninstalled successfully"

# Listing targets from user request
list_source:
	@mkdir -p $(LIST_DIR)
	@echo "Creating source code listing..."
	@> $(LIST_DIR)/listing.txt
	@find . -type d \( -name .unnat -o -name bin -o -name obj -o -name .vscode -o -name $(LIST_DIR) \) -prune -o \
	-type f \( -name "*.c" -o -name "*.h" -o -name "Makefile" -o -name "*.unna" \) -print | while read -r file; do \
		echo "=================================================================" >> $(LIST_DIR)/listing.txt; \
		echo "FILE: $$file" >> $(LIST_DIR)/listing.txt; \
		echo "=================================================================" >> $(LIST_DIR)/listing.txt; \
		cat "$$file" >> $(LIST_DIR)/listing.txt; \
		echo "" >> $(LIST_DIR)/listing.txt; \
	done
	@echo "Source code listing created at $(LIST_DIR)/listing.txt"

core_list:
	@mkdir -p $(LIST_DIR)
	@echo "Creating core source listing..."
	@> $(LIST_DIR)/corelist.txt
	@echo "=================================================================" >> $(LIST_DIR)/corelist.txt
	@echo "UNNARIZE CORE SOURCE LISTING" >> $(LIST_DIR)/corelist.txt
	@echo "=================================================================" >> $(LIST_DIR)/corelist.txt
	@echo "SECTION: include/ (Public Headers)" >> $(LIST_DIR)/corelist.txt
	@echo "=================================================================" >> $(LIST_DIR)/corelist.txt
	@find include -type f \( -name "*.h" -o -name "*.c" -o -name "*.unna" \) -print0 | sort -z | xargs -0 -I{} sh -c 'echo "=================================================================" >> $(LIST_DIR)/corelist.txt; echo "FILE: {}" >> $(LIST_DIR)/corelist.txt; echo "=================================================================" >> $(LIST_DIR)/corelist.txt; cat "{}" >> $(LIST_DIR)/corelist.txt; echo "" >> $(LIST_DIR)/corelist.txt'
	@echo "=================================================================" >> $(LIST_DIR)/corelist.txt
	@echo "SECTION: src/ (Interpreter Sources)" >> $(LIST_DIR)/corelist.txt
	@echo "=================================================================" >> $(LIST_DIR)/corelist.txt
	@find $(SRC_DIR) -type f \( -name "*.c" -o -name "*.h" -o -name "*.unna" \) -print0 | sort -z | xargs -0 -I{} sh -c 'echo "=================================================================" >> $(LIST_DIR)/corelist.txt; echo "FILE: {}" >> $(LIST_DIR)/corelist.txt; echo "=================================================================" >> $(LIST_DIR)/corelist.txt; cat "{}" >> $(LIST_DIR)/corelist.txt; echo "" >> $(LISTDIR)/corelist.txt'
	@echo "=================================================================" >> $(LIST_DIR)/corelist.txt
	@echo "SECTION: corelib/ (Standard Library)" >> $(LIST_DIR)/corelist.txt
	@echo "=================================================================" >> $(LIST_DIR)/corelist.txt
	@find corelib -type f \( -name "*.c" -o -name "*.h" \) -print0 | sort -z | xargs -0 -I{} sh -c 'echo "=================================================================" >> $(LIST_DIR)/corelist.txt; echo "FILE: {}" >> $(LIST_DIR)/corelist.txt; echo "=================================================================" >> $(LIST_DIR)/corelist.txt; cat "{}" >> $(LIST_DIR)/corelist.txt; echo "" >> $(LIST_DIR)/corelist.txt'
	@echo "=================================================================" >> $(LIST_DIR)/corelist.txt
	@echo "SECTION: Makefile (Build System)" >> $(LIST_DIR)/corelist.txt
	@echo "=================================================================" >> $(LIST_DIR)/corelist.txt
	@echo "FILE: Makefile" >> $(LIST_DIR)/corelist.txt
	@cat Makefile >> $(LIST_DIR)/corelist.txt
	@echo "" >> $(LIST_DIR)/corelist.txt
	@echo "Core source listing created at $(LIST_DIR)/corelist.txt"

.PHONY: all compile clean install uninstall list_source core_list