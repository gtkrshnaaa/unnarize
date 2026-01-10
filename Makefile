CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O3 -march=native -mtune=native -Iinclude -Icorelib/include -D_POSIX_C_SOURCE=200809L
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
# Manually list bytecode sources to avoid pattern rule conflicts
SOURCES_BYTECODE = $(SRCDIR)/bytecode/opcodes.c $(SRCDIR)/bytecode/chunk.c $(SRCDIR)/bytecode/compiler.c $(SRCDIR)/bytecode/interpreter.c

OBJECTS = $(SOURCES_SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o) \
          $(SOURCES_CORE:corelib/src/%.c=$(OBJDIR)/%.o) \
          $(OBJDIR)/opcodes.o \
          $(OBJDIR)/chunk.o \
          $(OBJDIR)/compiler.o \
          $(OBJDIR)/interpreter.o

EXECUTABLE = $(BINDIR)/unnarize

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: corelib/src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Explicit bytecode rules
$(OBJDIR)/opcodes.o: $(SRCDIR)/bytecode/opcodes.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/chunk.o: $(SRCDIR)/bytecode/chunk.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/compiler.o: $(SRCDIR)/bytecode/compiler.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/interpreter.o: $(SRCDIR)/bytecode/interpreter.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR) $(LISTDIR)

install: all
	@echo "Installing unnarize requires root privileges..."
	@echo "Use: sudo make install"
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(EXECUTABLE) $(DESTDIR)$(PREFIX)/bin/unnarize