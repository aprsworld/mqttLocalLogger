CC=gcc
CFLAGS=-I. -Wunused-function  -Wunused-variable -g

mqttLocalLogger: mqttLocalLogger.o 
	$(CC) mqttLocalLogger.o  -o mqttLocalLogger $(CFLAGS)  -lm -ljson-c -lmosquitto 


mqttLocalLogger.o: mqttLocalLogger.c
	$(CC)  -c mqttLocalLogger.c  $(CFLAGS) -I/usr/include/json-c/


clean:
	rm -f *.o
