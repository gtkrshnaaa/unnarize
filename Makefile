CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -D_POSIX_C_SOURCE=200809L

SRCDIR = src
OBJDIR = obj
BINDIR = bin
LISTDIR = z_listing

# Installation directories
PREFIX ?= /usr/local
DESTDIR ?=

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

EXECUTABLE = $(BINDIR)/unnarize

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ -ldl -lpthread -Wl,-export-dynamic

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR) $(LISTDIR)

# Install the unnarize binary to $(DESTDIR)$(PREFIX)/bin
install: all
	@echo "Installing unnarize requires root privileges..."
	sudo install -d $(DESTDIR)$(PREFIX)/bin
	sudo install -m 0755 $(EXECUTABLE) $(DESTDIR)$(PREFIX)/bin/unnarize
	@echo "Unnarize installed successfully to $(DESTDIR)$(PREFIX)/bin/unnarize"

# Remove the installed unnarize binary
uninstall:
	@echo "Uninstalling unnarize requires root privileges..."
	sudo rm -f $(DESTDIR)$(PREFIX)/bin/unnarize
	@echo "Unnarize uninstalled successfully"

run: all
	./$(EXECUTABLE) examples/test.unna

modularity-run: all
	./$(EXECUTABLE) examples/modularity/main.unna

arrays-maps-run: all
	./$(EXECUTABLE) examples/arraysMaps.unna

project-test-run: all
	./$(EXECUTABLE) examples/projectTest/main.unna

# Run async/await demo
async-demo-run: all
	./$(EXECUTABLE) examples/asyncDemo.unna

# Run all .unna files under examples/ (builds plugins first)
run-all: all build-plugins-all
	@echo "Running all examples..."
	@find examples -type f -name "*.unna" -print0 | sort -z | xargs -0 -I{} sh -c 'echo; echo ">>> Running: {}"; ./$(EXECUTABLE) "{}"'

PLUGINS_SRCDIR = examples/plugins/src
PLUGINS_BUILDDIR = examples/plugins/build

$(PLUGINS_BUILDDIR):
	mkdir -p $(PLUGINS_BUILDDIR)

# Build math plugin (.so)
build-plugin: all | $(PLUGINS_BUILDDIR)
	$(CC) -shared -fPIC -o $(PLUGINS_BUILDDIR)/mathLib.so $(PLUGINS_SRCDIR)/mathLib.c -lm -ldl

# Run demo that loads the math plugin and calls its function
run-math-lib-demo: build-plugin
	./$(EXECUTABLE) examples/plugins/demoMathLib.unna

# Build string plugin (.so)
build-plugin-strings: all | $(PLUGINS_BUILDDIR)
	$(CC) -shared -fPIC -o $(PLUGINS_BUILDDIR)/stringLib.so $(PLUGINS_SRCDIR)/stringLib.c -ldl

# Run demo for string plugin
run-string-lib-demo: build-plugin-strings
	./$(EXECUTABLE) examples/plugins/demoStringLib.unna

# Build time plugin (.so)
build-plugin-time: all | $(PLUGINS_BUILDDIR)
	$(CC) -shared -fPIC -o $(PLUGINS_BUILDDIR)/timeLib.so $(PLUGINS_SRCDIR)/timeLib.c -ldl

# Build timer plugin (.so) for benchmarking
build-plugin-timer: all | $(PLUGINS_BUILDDIR)
	$(CC) -shared -fPIC -o $(PLUGINS_BUILDDIR)/timerLib.so $(PLUGINS_SRCDIR)/timerLib.c -ldl

# Build all plugins
build-plugins-all: build-plugin build-plugin-strings build-plugin-time build-plugin-timer

# Run demo for time plugin
run-time-lib-demo: build-plugin-time
	./$(EXECUTABLE) examples/plugins/demoTimeLib.unna

# Run timer benchmark demo
run-timer-benchmark: build-plugin-timer
	./$(EXECUTABLE) examples/plugins/demoTimerLib.unna

# Build all plugins and run all demos
plugins-demo-run: build-plugins-all
	./$(EXECUTABLE) examples/plugins/demoMathLib.unna
	./$(EXECUTABLE) examples/plugins/demoStringLib.unna
	./$(EXECUTABLE) examples/plugins/demoTimeLib.unna

list_source:
	@mkdir -p $(LISTDIR)
	@echo "Creating source code listing..."
	@> $(LISTDIR)/listing.txt
	@find . -type d \( -name .unnat -o -name bin -o -name obj -o -name .vscode -o -name $(LISTDIR) \) -prune -o \
	-type f \( -name "*.c" -o -name "*.h" -o -name "Makefile" -o -name "*.unna" \) -print | while read -r file; do \
		echo "=================================================================" >> $(LISTDIR)/listing.txt; \
		echo "FILE: $$file" >> $(LISTDIR)/listing.txt; \
		echo "=================================================================" >> $(LISTDIR)/listing.txt; \
		cat "$$file" >> $(LISTDIR)/listing.txt; \
		echo "" >> $(LISTDIR)/listing.txt; \
	done
	@echo "Source code listing created at $(LISTDIR)/listing.txt"

.PHONY: all clean install uninstall run modularity-run arrays-maps-run project-test-run async-demo-run run-all list_source \
    build-plugin run-math-lib-demo build-plugin-strings run-string-lib-demo \
    build-plugin-time run-time-lib-demo build-plugins-all plugins-demo-run