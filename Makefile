CC=gcc
CFLAGS=-Ijson-c-0.14 -I. -Wunused-function  -Wunused-variable -g
LDFLAGS=-Ljson-c-build

SYS: mqttLocalLogger mqttLocalLoggerCSV test_jsonPath feedValuesToMQTT MqttJsonTransformer
	touch SYS

mqttLocalLogger: mqttLocalLogger.o 
	$(CC) mqttLocalLogger.o  -o mqttLocalLogger $(CFLAGS) $(LDFLAGS) -lm -ljson-c -lmosquitto 


mqttLocalLogger.o: mqttLocalLogger.c
	$(CC)  -c mqttLocalLogger.c  $(CFLAGS) -I/usr/include/json-c/


mqttLocalLoggerCSV: mqttLocalLoggerCSV.o 
	$(CC) mqttLocalLoggerCSV.o  -o mqttLocalLoggerCSV $(CFLAGS) $(LDFLAGS) -lm -ljson-c -lmosquitto -lncurses


mqttLocalLoggerCSV.o: mqttLocalLoggerCSV.c
	$(CC)  -c mqttLocalLoggerCSV.c  $(CFLAGS) -I/usr/include/json-c/

test_jsonPath:	test_jsonPath.o
	$(CC) test_jsonPath.o  -o test_jsonPath $(CFLAGS) $(LDFLAGS) -lm -ljson-c -lmosquitto 

test_jsonPath.o: test_jsonPath.c
	$(CC)  -c test_jsonPath.c  $(CFLAGS) -I/usr/include/json-c/

feedValuesToMQTT:	feedValuesToMQTT.o
	$(CC) feedValuesToMQTT.o  -o feedValuesToMQTT $(CFLAGS) $(LDFLAGS) -lm -lmosquitto 

feedValuesToMQTT.o: feedValuesToMQTT.c
	$(CC)  -c feedValuesToMQTT.c  $(CFLAGS) 

MqttJsonTransformer: MqttJsonTransformer.o
	$(CC) MqttJsonTransformer.o  -o MqttJsonTransformer $(CFLAGS) $(LDFLAGS) -lm -lmosquitto 

MqttJsonTransformer.o: MqttJsonTransformer.c
	$(CC)  -c MqttJsonTransformer.c  $(CFLAGS)  -I/usr/include/json-c/

clean:
	rm -f *.o
