# mqttLocalLogger

## Purpose

mqttLocalLogger subscribes to one or more mqtt topics and logs to one or multiple log files.


## Installation


`sudo apt-get install mosquitto-dev`

`sudo apt-get install libjson-c-dev`

`sudo apt-get install libmosquittopp-dev`

`sudo apt-get install libssl1.0-dev`

## Build

`make`


## Command line switches

switch|Required/Optional|argument|description
---|---|---|---
-t|REQUIRED|topic|mqtt topic
-s|OPTIONAL|text|end each log filename with suffix ie. ".json"   default="" 
-p|OPTIONAL|number|default is 1883
-l|OPTIONAL|path|logging derectory, default="logLocal"
-1|OPTIONAL|(none)|add logging will be done to one file in the logging derectory
-D|OPTIONAL|text|start each log filename with prefix rather than YYYYMMDD
-d|OPTIONAL|(none)|start each log file with YYYYMMDD, start new file each day.  (default) 
-f|OPTIONAL|(none)|unimplemented.	flush to log file after  each mqtt message.  (This is how it works).
-v|OPTIONAL|(none)|Turn on verbose (debuging).
-h|OPTIONAL|(none)|displays help and exits


## Discussion

There is one required argument and one required option.   The mqtt host must be on the command line.  Typically that is
is localhost.   The required option is -t (one or more times).

### Simplest example

`./mqttLocalLogger -t topic/left/right localhost`

This causes mqttLocalLogger to subscribe to the mqtt server on local host with the topic of `topic/left/right`.   Any messages on 
that topic will be logged to the file `./logLocal/topic/left/rigth/YYYYMMDD`, where if the current date is Christmas 2020 then 
YYYYMMDD="20201225".  Any packet received will be prepended with "2020-12-25 13:45:20," or whatever the current date and time.

### Adding -s .json

`./mqttLocalLogger -t topic/left/right -s .json localhost`

Nothing changes from the simplest example except that any messages on
that topic will be logged to the file `./logLocal/topic/left/rigth/YYYYMMDD.json`.

### Adding -l up/down

`./mqttLocalLogger -l updown -t topic/left/right localhost`

Nothing changes from the simplest example except that any messages on
that topic will be logged to the file `./up/down/topic/left/rigth/YYYYMMDD`.

### Adding -D BalloonData

`./mqttLocalLogger -D BalloonData -t topic/left/right localhost`

Nothing changes from the simplest example except that any messages on
that topic will be logged to the file `./logLocal/topic/left/rigth/BalloonData`.

### Adding -p 2025

Nothing changes from the simplest example except that  a connection is attempted to mqtt server
on port 2025 instead of 1883.

### An all together examples


`./mqttLocalLogger -t topic/left/right -t topic/right/left  -p 2025 -D BalloonData -l up/down -s .json localhost`

mqttLocalLogger will attempt to connect to the mqtt localhost server on port 2025. It will verify that `./up/down/` is usable and if 
not will attempt to fix it.  It will verify that `./up/down/topic/left/right` and `./up/down/topic/right/left` are usable and
attempt to fix any problems.  When data comes in depending on which topic the data will be written to the files 
`./up/down/topic/left/right/BalloonData.json` or `./up/down/topic/right/left/BalloonData.json`.

 
