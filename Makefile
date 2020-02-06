CC=gcc
CFLAGS=-I. -Wunused-function  -Wunused-variable -g

mqttLocalLogger: main.o 
	$(CC) main.o  -o mqttLocalLogger $(CFLAGS)  -lm -ljson-c -lmosquitto 


main.o: main.c
	$(CC)  -c main.c  $(CFLAGS) -I/usr/include/json-c/

