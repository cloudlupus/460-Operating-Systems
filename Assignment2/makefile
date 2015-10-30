CC = gcc
CFLAGS = -g -c -Wall

all: PageAlgs

PageAlgs: PageAlgs.o
	$(CC) -Wall PageAlgs.o MemSim64.o -o PageAlgs 

%.o: %.c
	$(CC) $(CFLAGS) $<

clean:
	rm -f PageAlgs.o PageAlgs