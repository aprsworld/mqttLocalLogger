# mqttLocalLogger

## mqttLocalLoggerCSV

Logs CSV to the logfile from the incoming json.
See [mqttLocalLoggerCSV.README.md](mqttLocalLoggerCSV.README.md) for details.

## test_jsonPath

Returns the json pointed to by jsonPath from the original json object.
See [test_jsonPath.README.md](test_jsonPath.README.md) for details.

## feedValuesToMQTT

Takes a column of raw data and turns them into json mqtt packets.
See [feedValuesToMQTT.README.md](feedValuesToMQTT.README.md) for details.
## Purpose

mqttLocalLogger subscribes to one or more mqtt topics and logs to one or multiple log files.


## Installation


`sudo apt-get install mosquitto-dev`

`sudo apt-get install libjson-c-dev`

`sudo apt-get install libmosquittopp-dev`

`sudo apt-get install libssl1.0-dev`

`sudo apt-get install libncurses-dev`

If the current linux version does not include json-c-0.14 or above then:

`git clone https://github.com/aprsworld/mqttLocalLogger.git`

`mkdir json-c-build`

`cd json-c-build`

`cmake ../json-c`

`make`

`cd ..`

`mv json-c json-c-0.14`


## Build

`make`


## Command line switches

switch|Required/Optional|argument|description
---|---|---|---
--mqtt-topic|REQUIRED|topic|mqtt topic
--mqtt-status-topic|OPTIOBAL|topic|mqtt topic for logging the status of the logger
--log-file-suffix|OPTIONAL|text|end each log filename with suffix ie. ".json"   default="" 
--mqtt-port|OPTIONAL|number|default is 1883
--log-dir|OPTIONAL|path|logging derectory, default="logLocal"
--unitary-log-file|OPTIONAL|(none)|all logging will be done to one file in the logging derectory
--log-file-prefix|OPTIONAL|text|start each log filename with prefix rather than YYYYMMDD
--split-log-file-by-day|OPTIONAL|(none)|start each log file with YYYYMMDD, start new file each day.  (default) 
--verbose|OPTIONAL|(none)|Turn on verbose (debuging).
--help|OPTIONAL|(none)|displays help and exits


## Discussion

There is one required argument and one required option.   The mqtt host must be on the command line.  Typically that is
is localhost.   The required option is -t (one or more times).

### Simplest example

`./mqttLocalLogger --mqtt-topic topic/left/right --mqtt-host localhost`

This causes mqttLocalLogger to subscribe to the mqtt server on local host with the topic of `topic/left/right`.   Any messages on 
that topic will be logged to the file `./logLocal/topic/left/rigth/YYYYMMDD`, where if the current date is Christmas 2020 then 
YYYYMMDD="20201225".  Any packet received will be prepended with "2020-12-25 13:45:20," or whatever the current date and time.

### Adding --log-file-suffix .json

`./mqttLocalLogger --mqtt-topic topic/left/right --log-file-suffix .json --mqtt-host localhost`

Nothing changes from the simplest example except that any messages on
that topic will be logged to the file `./logLocal/topic/left/rigth/YYYYMMDD.json`.

### Adding --log-dir up/down

`./mqttLocalLogger --log-dir updown --mqtt-topic topic/left/right --mqtt-host localhost`

Nothing changes from the simplest example except that any messages on
that topic will be logged to the file `./up/down/topic/left/rigth/YYYYMMDD`.

### Adding --log-file-prefix BalloonData

`./mqttLocalLogger --log-file-prefix BalloonData --mqtt-topic topic/left/right --mqtt-host localhost`

Nothing changes from the simplest example except that any messages on
that topic will be logged to the file `./logLocal/topic/left/rigth/BalloonData`.

### Adding --mqtt-port 2025

Nothing changes from the simplest example except that  a connection is attempted to mqtt server
on port 2025 instead of 1883.

### An all together examples


`./mqttLocalLogger --mqtt-topic topic/left/right --mqtt-topic topic/right/left  --mqtt-port 2025 --log-file-prefix BalloonData --log-dir up/down --log-file-suffix .json --mqtt-hot localhost`

mqttLocalLogger will attempt to connect to the mqtt localhost server on port 2025. It will verify that `./up/down/` is usable and if 
not will attempt to fix it.  It will verify that `./up/down/topic/left/right` and `./up/down/topic/right/left` are usable and
attempt to fix any problems.  When data comes in depending on which topic the data will be written to the files 
`./up/down/topic/left/right/BalloonData.json` or `./up/down/topic/right/left/BalloonData.json`.
