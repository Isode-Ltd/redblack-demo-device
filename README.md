### Compiling Go based web device

#### Http Router
Go module https://github.com/julienschmidt/httprouter needs to be installed locally.
```bash
$ go get github.com/julienschmidt/httprouter
```

#### GO MOD INIT
```bash
$ go mod init <module_name>
```

#### Compile the sample web application i.e a demo / dummy radio device
```bash
$ go build web_device.go
```

#### Get the web application for the dummy radio device running

```bash
$ ./web_device
```

#### Connect to the dummy radio device using browser

```bash
Browser URL : http://localhost:8080/view/radio
```

#### Fetch the status of the device
Example:
```bash

$ curl -X GET "http://localhost:8080/device/radio/ref"
{"Alert":"INFO","DeviceTypeHash":"#ABCDE","RunningSince":"1m11.795971925s","StartTime":"2021-06-03 17:34:40","Status":"Enabled","UniqueId":"1232","Version":"1.0"}

$ curl -X GET "http://localhost:8080/device/radio/status"
{"PowerSupplyConsumption":"300","PowerSupplyVoltage":"20","SignalLevel":"500","Temperature":"40","VSWR":"10"}

$ curl -X GET "http://localhost:8080/device/radio"
{"VSWR":"10","PowerSupplyVoltage":"20","PowerSupplyConsumption":"300","Temperature":"40","SignalLevel":"500","Frequency":"11015","TransmissionPower":"7528","Modem":"","Antenna":"","DeviceType":"radio","Status":"Enabled","StartTime":"2021-06-03 17:34:40","RunningSince":"1m34.998094147s","Version":"1.0","Alert":"INFO","DeviceTypeHash":"#ABCDE","UniqueId":"1232","DeviceDescription":""}

$ curl -X GET "http://localhost:8080/device/radio/param/signallevel"
"5"

$ curl -X GET "http://localhost:8080/device/radio/param/vswr"
"50"
```

#### Set the control parameters of the device
Example:
```bash
$ $ curl --header "Content-Type: application/json" -X POST "http://localhost:8080/device/radio/control" --data '{"Frequency":"26000","TransmissionPower":"8000", "Modem":"Audio", "Antenna":"RF"}'
```
View the updated parameters
```bash
Browser URL : http://localhost:8080/view/radio
```

### Compiling C++ driver

Run the below commands inside the driver directory.
```bash
rb_device/driver$ cmake .
rb_device/driver$ make
```
### Testing the C++ driver with the dummy go device

redblack$ build/sampledrivers/dummygodev "Radio" "schema/isode-radio.xml" "schema/stdparams.xml" | /workspace/red_black/rb_device/driver/driver --host localhost --port 8080 radio schema/isode-radio.xml schema/stdparams.xml
```
