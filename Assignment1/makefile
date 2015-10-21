CC = gcc
CFLAGS = -g -c -Wall

all: SCHEDULER

SCHEDULER: SCHEDULER.o
	$(CC) -Wall SCHEDULER.o SchedSim.o -o SCHEDULER 

%.o: %.c
	$(CC) $(CFLAGS) $<

clean:
	rm -f SCHEDULER.o SCHEDULER