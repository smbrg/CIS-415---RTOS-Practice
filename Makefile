OBJECTS3 = p1fxns.o uspsv3.o
CFLAGS = -W -Wall -g
CC = gcc

all: uspsv3

uspsv3: $(OBJECTS3)
	$(CC) -o uspsv3 $(CFLAGS) $(OBJECTS3)

p1fxns.o: p1fxns.c p1fxns.h

uspsv3.o: uspsv3.c

clean:
	rm *.o uspsv3
