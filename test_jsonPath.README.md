# test_jsonPath

## Purpose

This program verifies that your jsonPath works with your jsonObject.

## Running

`./test_jsonPath $jsonOject $jsonPath`

This must conform to RFC 6901.   
## $jsonOject

is a string and not a file.  If you have it in a file you can convert to a string by
`export jsonObject=cat somefile`

Lets assume that $jsonObject is:
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

The first thing the program does is try converting $jsonObject  into a real jsonObject held internally.   If this step fails you
will get a message  `not a json object`.

## $jsonPath

`""` is a valid jsonPath and will return the entire $jsonObject and `# 0` meaning no error.   Try this so you understand it but
it is probably not what you want.

"/date" is a valid jsonPath.  The program will return:
```
# jsonPath  = '/date'
# json_pointer_get() output = "2020-05-08 14:57:07.863"
# 0
```

If you want `-65.859031000000002` then try this for jsonPath `/formattedData/WC/WeighContinuous/value`.  The program will return:
```
# jsonPath  = '/formattedData/WC/WeighContinuous/value'
# json_pointer_get() output = -65.859031000000002
# 0
```



