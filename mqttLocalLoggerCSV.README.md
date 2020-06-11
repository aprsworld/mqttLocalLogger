# mqttLocalLoggerCSV

## Purpose

mqttLocalLoggerCSV subscribes to one or more mqtt topics and logs to one csv file.  
The configuration files contains information on subscription topics, columns, and methods.
If the --display-herz is used then data is not logged but is displayed to the screen using ncurses.  


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
--mqtt-host|REQUIRED||Is a qualified host or ip address that has moquitto server.
--hertz|OPTIONAL|integer|Default hertz = 1.
--millisecond-interval|OPTIONAL|number|Number of mSeconds between output lines.  Not required if --herz used.
--display-hertz|OPTIONAL|number|Turns off logging and displays to screen formatted
--log-file-suffix|OPTIONAL|text|end each log filename with suffix ie. ".csv"   default=".csv" 
--mqtt-port|OPTIONAL|number|default is 1883
--log-dir|OPTIONAL|path|logging derectory, default="logLocal"
--log-file-prefix|OPTIONAL|text|start each log filename with prefix rather than YYYYMMDD
--verbose|OPTIONAL|(none)|Turn on verbose (debuging).
--help|OPTIONAL|(none)|displays help and exits


## Discussion

All of the fun stuff occurs in --configuration file.

### Simplest example

`./mqttLocalLoggerCSV --mqtt-host localhost --configuration 123.conf`

This causes mqttLocalLoggerCSV to  read the configuration file and subscribe to the mqtt server on local host with the topics
described in the configuration file. Any messages on 
the topics will be logged to the file `./logLocal/YYYYMMDD.csv`, where if the current date is Christmas 2020 then 
YYYYMMDD="20201225".  Any packet received will be prepended with "2020-12-25 13:45:20," or whatever the current date and time.

### Adding --log-dir up/down

`./mqttLocalLoggerCSV --log-dir up/down --mqtt-host localhost --configuration 123.conf`

Nothing changes from the simplest example except that any messages on
that topic will be logged to the file `./up/down/YYYYMMDD.csv`.

### Adding --log-file-prefix BalloonData

`./mqttLocalLoggerCSV --log-file-prefix BalloonData --mqtt-host localhost --configuration 123.conf`

Nothing changes from the simplest example except that any messages on
that topic will be logged to the file `./logLocal/BalloonData.csv`.

### Adding --mqtt-port 2025

Nothing changes from the simplest example except that  a connection is attempted to mqtt server
on port 2025 instead of 1883.

## configuration

A configuration file is written in json format and contains a json array labeled columns.

```
{
    "columns": [
	{
        "csvColumn": "B",
        "csvOutput": "value",
        "csvTitle": "date",
        "mqttTopic": "left/WC",
        "jsonPath": "/date"
	    },
	{
        "csvColumn": "C",
        "csvOutput": "value",
        "csvTitle": "WeighContinuousValue",
        "mqttTopic": "left/WC",
        "jsonPath": "/formattedData/WC/WeighContinuous/value"
	    },
	]
} 
```
## csvColumn

Must be unique for each column and one or 2 upper case letters.  `"A"` is reserved the for the program supplied
date, so the range is from `"B"` to `"ZZ"`.   A sparce matrix or gaps are allowed.  The order of the columns in the
configuration file is unimportant to the program as it will output in column order.

## csvOutput

Determines the type of output.

### value

will output as a string, whatever the elements that is pointed to.

### value_integer

will output as a integer, whatever the elements that is pointed to.

### value_double

will output as a double, whatever the elements that is pointed to.

### count

will output as an integer, the number of times the element that was pointed to occurred.

### sum

will output  as a double the sum of the elements that occurred for each element pointed to.   ie.  5 5 5   putputs 15.000

### mean

will output as a double the mean of the elements that occurred for each element pointed to.   ie. 4 5 6 outputs 5.000

### standard-deviation 

will output as a double the standard deviation of the elements that occurred for each element pointed to.

### maximum

will output as at double, the largest value that occurred in the period.

### minimum

will output as a double, the smallest value that occurred in the period.

## csvTitle

When the program starts the first line in the csv ouput will look like this:
```

DATE,date,WeighContinuousValue,
```
combining `DATE` with column B csvTitle and column C csvTitle.

## mqttTopic

This must contain the exact topic desired.   These are not required to be uniform or unique.   Any valid topic is allowed
but remember it must be available on the host or no usable data will be output.


## jsonPath

The jsonPath is required but maybe empty if you are using csvOutputFormat as a literal and the csvColumn is also
empty.

This must conform to RFC 6901.   I will explain the basic using the example below:
```
{
  "date":"2020-05-08 14:57:07.863",
  "milliSecondSinceStart":0,
  "rawData":"  -65.859031",
  "formattedData":{
    "WC":{
      "messageType":"WC",
      "WeighContinuous":{
        "value":-65.859031000000002
      }
    }
  }
}
```
### /date

Will point to `"2020-05-08 14:57:07.863"`.   Since this is just a string you can use `value` or `count`.

### /formattedData

Will point to a json object and you probably don't want that.

### /formattedData/WC/WeighContinuous/value

will point to `-65.859031000000002`.   Since this a numeric item you can use all csvOuput including
`value`, `count`, `sum`, `mean`, and `standard-deviation`.

## csvTitleY, csvTitleX, csvAgerY, csvAgerX, csvOutputY, csvOutputX

### csvTitleY

Is the Y coordinate on the screen.   When --display-hertz is used then this determines the
row for the csvTitle.

### csvTitleX

Is the X coordinate on the screen.   When --display-hertz is used then this determines the
column for the csvTitle.

### csvAgerY

Is the Y coordinate on the screen.   When --display-hertz is used then this determines the
the row to post the data-age in seconds.

### csvAgerX

Is the X coordinate on the screen.   When --display-hertz is used then this determines the
the cloumn to post the data-age in seconds.

### csvOutputY

Is the Y coordinate on the screen.   When --display-hertz is used then this determines the
the row to post the csvOutput.

### csvOutputX

Is the X coordinate on the screen.   When --display-hertz is used then this determines the
the column to post the csvOutput.

## csvOutputFormat

Will apply a printf like formatting to the data in the column.   This format will pertain to data displayed on a screen
or written to the CSV logfile.   Typical formats might be `"%8.8s"`, `"%06d"``,`"%9.2lf"`, etc.

If you use a format `"$%3.2lf"` and you do not want the `$` to show in the csv file then you must create a column entry where
csvColumn=`""`, and the format to be `"$"`.   Then use "%9.2lf"`  in the entry that has a non-empty csvColumn.

Simply put anything that is to be excluded from the csv file must have an empty csvColumn and may have an empty jsonPath.

