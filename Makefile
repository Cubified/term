all: term

CC=gcc

LIBS=-lX11
CFLAGS=-Os -pipe -s -pedantic
DEBUGCFLAGS=-Og -pipe -g -Wall -Wextra

INPUT=term.c
OUTPUT=term

RM=/bin/rm

.PHONY: term
term:
	$(CC) $(INPUT) -o $(OUTPUT) $(LIBS) $(CFLAGS)

debug:
	$(CC) $(INPUT) -o $(OUTPUT) $(LIBS) $(DEBUGCFLAGS)

clean:
	if [ -e $(OUTPUT) ]; then $(RM) $(OUTPUT); fi
