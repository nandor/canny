# Makefile for hog

CC=gcc
CFLAGS=-c -pedantic -Wall -Wextra -std=c99 -O2 -funroll-loops -g
LDFLAGS=-lc -lm -lX11 -lGLEW -lglfw -lGL
SOURCES=main.c camera.c window.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=hog

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(EXECUTABLE)