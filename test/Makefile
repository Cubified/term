all: test

CC=gcc

LIBS=-lm
CFLAGS=-Os -pipe -s -pedantic

.PHONY: test
test:
	$(CC) test_esc.c -o test_esc $(LIBS) $(CFLAGS)
	$(CC) truecolor_stresstest.c -o truecolor_stresstest $(LIBS) $(CFLAGS)
