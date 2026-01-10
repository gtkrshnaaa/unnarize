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
SOURCES_BYTECODE = $(SRCDIR)/bytecode/opcodes.c $(SRCDIR)/bytecode/chunk.c $(SRCDIR)/bytecode/compiler.c $(SRCDIR)/bytecode/interpreter.c

OBJECTS = $(SOURCES_SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o) \
          $(SOURCES_CORE:corelib/src/%.c=$(OBJDIR)/%.o) \
          $(OBJDIR)/opcodes.o $(OBJDIR)/chunk.o $(OBJDIR)/compiler.o $(OBJDIR)/interpreter.o

EXECUTABLE = $(BINDIR)/unnarize

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) | $(BINDIR)
\t$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
\t$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: corelib/src/%.c | $(OBJDIR)
\t$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/opcodes.o: $(SRCDIR)/bytecode/opcodes.c | $(OBJDIR)
\t$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/chunk.o: $(SRCDIR)/bytecode/chunk.c | $(OBJDIR)
\t$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/compiler.o: $(SRCDIR)/bytecode/compiler.c | $(OBJDIR)
\t$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/interpreter.o: $(SRCDIR)/bytecode/interpreter.c | $(OBJDIR)
\t$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
\tmkdir -p $(OBJDIR)

$(BINDIR):
\tmkdir -p $(BINDIR)

clean:
\trm -rf $(OBJDIR) $(BINDIR) $(LISTDIR)