CC=gcc
CFLAGS=-I.

.PHONY: all clean

all: ccos_disk_tool

main.o: main.c ccos_image.h
	$(CC) -c main.c $(CFLAGS)

ccos_image.o: ccos_image.c ccos_image.h
	$(CC) -c ccos_image.c $(CFLAGS)

ccos_disk_tool: ccos_image.o main.o
	$(CC) -o ccos_disk_tool ccos_image.o main.o

clean:
	rm ccos_disk_tool *.o
