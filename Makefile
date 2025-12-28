CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O3 -Iinclude -Icorelib/include -D_POSIX_C_SOURCE=200809L
LDFLAGS = -ldl -lpthread -Wl,-export-dynamic

SRCDIR = src
OBJDIR = obj
BINDIR = bin
LISTDIR = z_listing

# Installation directories
PREFIX ?= /usr/local
DESTDIR ?=

SOURCES_SRC = $(wildcard $(SRCDIR)/*.c)
SOURCES_CORE = $(wildcard corelib/src/*.c)
OBJECTS = $(SOURCES_SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o) $(SOURCES_CORE:corelib/src/%.c=$(OBJDIR)/%.o)

EXECUTABLE = $(BINDIR)/unnarize

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: corelib/src/%.c | $(OBJDIR)
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
	./$(EXECUTABLE) examples/01_basics.unna

run-basics: all
	./$(EXECUTABLE) examples/01_basics.unna

run-structures: all
	./$(EXECUTABLE) examples/02_structures.unna

run-functions: all
	./$(EXECUTABLE) examples/03_functions.unna

run-async: all
	./$(EXECUTABLE) examples/04_async.unna

run-modules: all
	./$(EXECUTABLE) examples/05_modules.unna

run-operators: all
	./$(EXECUTABLE) examples/06_operators.unna

run-foreach: all
	./$(EXECUTABLE) examples/07_foreach.unna

run-structs: all
	./$(EXECUTABLE) examples/08_structs.unna

run-errors: all
	./$(EXECUTABLE) examples/09_errors.unna

run-uon: all
	./$(EXECUTABLE) examples/10_ucore_uon.unna

run-uon-file: all
	./$(EXECUTABLE) examples/11_uon_file.unna

run-http: all
	./$(EXECUTABLE) examples/13_http.unna

run-server: all
	./$(EXECUTABLE) examples/14_server.unna

# Run all .unna files under examples/ (excluding modules subfolder implicitly if not executed directly, but find will find them)
# We usually only want to run top-level examples.
run-all: all build-plugins-all
	@echo "Running all top-level examples..."
	@find examples -maxdepth 1 -type f -name "*.unna" -print0 | sort -z | xargs -0 -I{} sh -c 'echo; echo ">>> Running: {}"; ./$(EXECUTABLE) "{}"'

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

# Run all documentation-sourced Unnarize examples under examples/docssource/
run-docssource: all build-plugins-all
	@echo "Running documentation examples from examples/docssource..."
	@find examples/docssource -type f -name "*.unna" -print0 | sort -z | xargs -0 -I{} sh -c 'echo; echo ">>> Running: {}"; ./$(EXECUTABLE) "{}"'

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

core_list:
	@mkdir -p $(LISTDIR)
	@echo "Creating core source listing..."
	@> $(LISTDIR)/corelist.txt
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@echo "UNNARIZE CORE SOURCE LISTING" >> $(LISTDIR)/corelist.txt
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@echo "This document lists the core of the Unnarize programming language: design philosophy, public headers (include/)," >> $(LISTDIR)/corelist.txt
	@echo "interpreter sources (src/), language examples (examples/), and the build system (Makefile)." >> $(LISTDIR)/corelist.txt
	@echo "" >> $(LISTDIR)/corelist.txt
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@echo "SECTION: CORE PHILOSOPHY (COREPHILOSOPHY.md)" >> $(LISTDIR)/corelist.txt
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@echo "FILE: COREPHILOSOPHY.md" >> $(LISTDIR)/corelist.txt
	@cat COREPHILOSOPHY.md >> $(LISTDIR)/corelist.txt || true
	@echo "" >> $(LISTDIR)/corelist.txt
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@echo "SECTION: include/ (Public Headers)" >> $(LISTDIR)/corelist.txt
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@find include -type f \( -name "*.h" -o -name "*.c" -o -name "*.unna" \) -print0 | sort -z | xargs -0 -I{} sh -c 'echo "=================================================================" >> $(LISTDIR)/corelist.txt; echo "FILE: {}" >> $(LISTDIR)/corelist.txt; echo "=================================================================" >> $(LISTDIR)/corelist.txt; cat "{}" >> $(LISTDIR)/corelist.txt; echo "" >> $(LISTDIR)/corelist.txt'
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@echo "SECTION: src/ (Interpreter Sources)" >> $(LISTDIR)/corelist.txt
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@find $(SRCDIR) -type f \( -name "*.c" -o -name "*.h" -o -name "*.unna" \) -print0 | sort -z | xargs -0 -I{} sh -c 'echo "=================================================================" >> $(LISTDIR)/corelist.txt; echo "FILE: {}" >> $(LISTDIR)/corelist.txt; echo "=================================================================" >> $(LISTDIR)/corelist.txt; cat "{}" >> $(LISTDIR)/corelist.txt; echo "" >> $(LISTDIR)/corelist.txt'
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@echo "SECTION: corelib/ (Standard Library)" >> $(LISTDIR)/corelist.txt
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@find corelib -type f \( -name "*.c" -o -name "*.h" \) -print0 | sort -z | xargs -0 -I{} sh -c 'echo "=================================================================" >> $(LISTDIR)/corelist.txt; echo "FILE: {}" >> $(LISTDIR)/corelist.txt; echo "=================================================================" >> $(LISTDIR)/corelist.txt; cat "{}" >> $(LISTDIR)/corelist.txt; echo "" >> $(LISTDIR)/corelist.txt'
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@echo "SECTION: examples/ (Language Examples)" >> $(LISTDIR)/corelist.txt
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@find examples -type d \( -name .unnat -o -name bin -o -name obj -o -name .vscode -o -name $(LISTDIR) \) -prune -o -type f \( -name "*.unna" -o -name "*.c" -o -name "*.h" \) -print0 | sort -z | xargs -0 -I{} sh -c 'echo "=================================================================" >> $(LISTDIR)/corelist.txt; echo "FILE: {}" >> $(LISTDIR)/corelist.txt; echo "=================================================================" >> $(LISTDIR)/corelist.txt; cat "{}" >> $(LISTDIR)/corelist.txt; echo "" >> $(LISTDIR)/corelist.txt'
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@echo "SECTION: Makefile (Build System)" >> $(LISTDIR)/corelist.txt
	@echo "=================================================================" >> $(LISTDIR)/corelist.txt
	@echo "FILE: Makefile" >> $(LISTDIR)/corelist.txt
	@cat Makefile >> $(LISTDIR)/corelist.txt
	@echo "" >> $(LISTDIR)/corelist.txt
	@echo "Core source listing created at $(LISTDIR)/corelist.txt"

.PHONY: all clean install uninstall run modularity-run arrays-maps-run project-test-run async-demo-run run-all run-docssource list_source core_list \
    build-plugin run-math-lib-demo build-plugin-strings run-string-lib-demo \
    build-plugin-time run-time-lib-demo build-plugins-all plugins-demo-run