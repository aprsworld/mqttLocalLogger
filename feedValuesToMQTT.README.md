# feedValuesToMQTT

Purpose is to feed a file containing one numeric value per line, turning it into simple json and
having the known values rapidly published.

## Installation


`sudo apt-get install mosquitto-dev`

`sudo apt-get install libmosquittopp-dev`

`sudo apt-get install libssl1.0-dev`


## Build

`make`


## Command line switches

switch|Required/Optional|argument|description
---|---|---|---
--mqtt-host|REQUIRED||Is a qualified host or ip address that has moquitto server.
--mqtt-port|OPTIONAL|number|default is 1883
--input-file|REQUIRED|file name|a text file with one number per line.
--verbose|OPTIONAL|(none)|Turn on verbose (debuging).
--quiet|OPTIONAL|(none)|Turn off stdout.
--help|OPTIONAL|(none)|displays help and exits


## Example

`./feedValuesToMQTT --mqtt-host localhost --mqtt-topic whatever --input-file rosebush.dat`

mqtt published packet will look like this 
```i
{ "value": 23.345 }
```
