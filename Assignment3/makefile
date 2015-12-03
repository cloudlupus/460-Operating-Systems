CC = gcc
CFLAGS = -g -c -Wall

all: EasyTest

EasyTest: EasyTest.o myFS.o Driver.o
	$(CC) -Wall EasyTest.o myFS.o Driver.o -o EasyTest

myFS: myFS.o Driver.o
	$(CC) -Wall myFS.o Driver.o -o myFS

%.o: %.c
	$(CC) $(CFLAGS) $<

clean:
	rm -f myFS.o myFS