# Makefile for hog

CC=gcc
CFLAGS=-c -Wall -Wextra -std=gnu99 -O2 -funroll-loops -g
LDFLAGS=-lc -lm -lX11 -lGLEW -lGL -lOpenCL
SOURCES=main.c camera.c window.c process.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=canny

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): program.h $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

program.h:
	xxd -i program.cl > program.h

clean:
	rm -rf *.o $(EXECUTABLE) program.h

