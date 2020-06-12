# mqttLocalLoggerCSV example configurations

## GrandSlam.conf

This configuration demonstrates the types of information that can be combined into one csv log file.  Essentially
all data can be collected into to one file from all the instruments that are on line, reporting via mqtt in a json format.
This automatically does the real time synchonization of the data.

### mqttTopic
in this file we are listening for four different mqtt topics:
```
        "mqttTopic": "airmar/GPGGA",
        "mqttTopic": "airmar/WIMDA",
        "mqttTopic": "left/WC",
        "mqttTopic": "right",
```

Two of topics are coming from an airmar device but are of different NEAMA sentences.  The other topics are coming from separat
devices.  

### jsonPath
Since each device may have a different json structure the jsonPath tells the program where in the json object the
data is kept.

```
        "jsonPath": "/date"
        "jsonPath": "/formattedData/globalPositioningSystemFixData/latitude/value"
        "jsonPath": "/formattedData/globalPositioningSystemFixData/longitude/value"
        "jsonPath": "/formattedData/meteorloicalComposite/barometricPreasureInches/value"
        "jsonPath": "/formattedData/WC/WeighContinuous/value"
        "jsonPath": "/iMet_XQ2_FORMAT/temperature/value"
```

This must conform to RFC 6901.   

The `/` means to start with the base json object.   

if you have trouble getting this working use test_jsonPath.
See [test_jsonPath.README.md](test_jsonPath.README.md) for details.


### csvColumn

```
        "csvColumn": "",
        "csvColumn": "B",
        "csvColumn": "C",
        "csvColumn": "D",
	.
	.
	.
        "csvColumn": "ZZ",
```

All columns must have a json element called `csvColumn`.   This element can be empty but is required.  The range of
non-empty elements is '`B` through `ZZ` and must be unique.   The columns in the configuration file do NOT have to be in
order, as the program sorts them before acting.

### csvOutput
```
        "csvOutput": "count",
        "csvOutput": "mean",
        "csvOutput": "standard_deviation",
        "csvOutput": "sum",
        "csvOutput": "value",
        "csvOutput": "value_double",
        "csvOutput": "value_int",
```

We now support three instances of value.   If jsonPath points to a floating point number then use `value_double`.  If
sonPath points to a signed or unsigned integer then use `value_int`.   Anything else use `value`.

`count` returns an unsigned integer.  `mean`, `standard_deviation`, `sum`, `maximum`, `minimum` all return double.

### csvTitle

`csvTitle` is treated as a string and is written to the first csv record output in the appropriate column.  Every
time mqttLocalLoggerCSV is started a new header is written.


## mqttLocalLoggerCSV.example.conf

`GrandSlam.conf` uses a subset of the configuration rules that pertain only to the writting of the csv file.  mqttLocalLoggerCSV.example.conf has configuration rules that pertain to displaying results on the screen is real time.  Here is the new stuff.

### csvOutputFormat

`csvOutputFormat` is completely optional.  It basically has two uses.  First it can limit the rediculous long output of json and it can
write labels to the screen.  if the `csvColumn` is not empty then data will be written to the csv file using that format.
if `csvColumn` is emptry then the format will not be used write to the csv file but will be used to format to the display screen.

To truncate or write data to a particular format 
```
	"csvOutputFormat": "%5.2lf",
	"csvOutputFormat": "%6.3lf",
	"csvOutputFormat": "%8.2lf",
	"csvOutputFormat": "%8.3lf",
	"csvOutputFormat": "%8.4lf",
```

This conforms to the rules used by snprintf().  If the output is a count or value_integer then use 
```
	"csvOutputFormat": "%d",
```

If you just want a label on the screen then
```
	"csvOutputFormat": "(c) APRSWORLD, LLC.",
```

###  csvAgerY csvAgerX


```
	"csvAgerY": 2,
	"csvAgerX": 100,
```

Puts the age in the `"%6.3lf"` format in seconds, so to the right of the decimal point is milliseconds.   There is no way to attach
a label to this directly.

### csvTitleY csvTitleX

```
	"csvTitleY": 2,
	"csvOutputX": 20,
        "csvTitle": "pressure",
```

Puts the word `pressure` at y=2, x=20.  That is the third row and the twenty-first column. on the screen.


### csvOutputY csvOutputX

```
        "csvOutput": "value_double",
	"csvOutputY": 8,
	"csvOutputX": 20,
	"csvOutputFormat": "%6.3lf",
```

Puts the double precision value at y=8, x=20.   That is the ninth row and the twenty-first column. on the screen.
There will be three digits to the right of the decimal point.

