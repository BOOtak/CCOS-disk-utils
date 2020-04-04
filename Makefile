CC=gcc
CFLAGS=-I. -O0 -g -Werror -Wall

.PHONY: all clean

all: ccos_disk_tool

common.o: common.c common.h
	$(CC) -c common.c $(CFLAGS)

wrapper.o: wrapper.c wrapper.h string_utils.h ccos_image.h common.h
	$(CC) -c wrapper.c $(CFLAGS)

string_utils.o: string_utils.c string_utils.h
	$(CC) -c string_utils.c $(CFLAGS)

main.o: main.c ccos_image.h wrapper.h common.h
	$(CC) -c main.c $(CFLAGS)

ccos_image.o: ccos_image.c ccos_image.h common.h
	$(CC) -c ccos_image.c $(CFLAGS)

ccos_disk_tool: ccos_image.o main.o string_utils.o wrapper.o common.o
	$(CC) -o ccos_disk_tool ccos_image.o main.o string_utils.o wrapper.o common.o

clean:
	rm ccos_disk_tool *.o
