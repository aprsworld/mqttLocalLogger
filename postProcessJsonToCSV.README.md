# postProcessJsonToCSV

## Purpose

postProcessJsonToCSV allows the build of a csv file from the same configuration as used by
mqttLocalLoggerCSV.   It is freed from the constrains of operating in real time, so all of the
records are process in correct order and not records are missed because the did not make the
real time cutoff.


## Installation


`sudo apt-get install mosquitto-dev`

`sudo apt-get install libjson-c-dev`

`sudo apt-get install libmosquittopp-dev`

`sudo apt-get install libssl1.0-dev`

If libjson-c does not support json_pointer.h than a more current version will need to be downloaded and
the necessary work for statically linking to the local build will be necessary.

## Build

`make`


## Command line switches

switch|Required/Optional|argument|description
---|---|---|---
--configuration|REQUIRED|text|Contains json array of columns defining i/o
--input-file-name|REQUIRED|basename|ie 20200813.gz
--output-file-name|REQUIRED|relative to log-dir or absolute
--log-dir|OPTIONAL|path|logging derectory, default="logLocal"
--verbose|OPTIONAL|(none)|Turn on verbose (debuging).
--progress-indicator|OPTIONAL|(none)|reports every 1000 records
--help|OPTIONAL|(none)|displays help and exits

## --configuration

Logs CSV to the logfile from the incoming json.
See [mqttLocalLoggerCSV.README.md](mqttLocalLoggerCSV.README.md) for details.

## --input-file-name

Hopefully you are using the same log naming convention for each log created my mqttLocalLogger.
See [README.md](README.md) for details.

At the beinging of the next day the logs are compressed using gzip.   This puts the `.gz` suffix on
each log file.   The default name is yyyymmdd so when compressed it becomes yyyymmdd.gz.  ie `20200813.gz`.


## --output-file-name

It is really important that existing data does not get accidentally modified.  Therefore the output csv name
must be unique.  I suggest post.yyyymmdd.csv where yyyymmdd is the date of the original data.  You can
also you a name with an abosulte path.

## --log-dir

Logs CSV to the logfile from in --log-dir
See [README.md](README.md) for details.

## --verbose

Adds runtime na debugging info to stderr.

## --progress-indicator

Add a report every 1,000 input records and reports the count and the timestamp being processed.

## example

` ./postProcessJsonToCSV --log-dir production/logLocal/ --input-file 20200810.gz --configuration production/LOG_ALL.conf --progress-indicator --output-file-name example.csv`

