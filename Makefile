CC=gcc
CFLAGS=-I. -O0 -g

.PHONY: all clean

all: ccos_disk_tool

dumper.o: dumper.c dumper.h string_utils.h ccos_image.h
	$(CC) -c dumper.c $(CFLAGS)

string_utils.o: string_utils.c string_utils.h
	$(CC) -c string_utils.c $(CFLAGS)

main.o: main.c ccos_image.h dumper.h
	$(CC) -c main.c $(CFLAGS)

ccos_image.o: ccos_image.c ccos_image.h
	$(CC) -c ccos_image.c $(CFLAGS)

ccos_disk_tool: ccos_image.o main.o string_utils.o dumper.o
	$(CC) -o ccos_disk_tool ccos_image.o main.o string_utils.o dumper.o

clean:
	rm ccos_disk_tool *.o
