## Radio Device
This web device emulates isoderadio. Just like an actual radio device, the web-device has control and status parameters. The control parameters could be modified using a Red-Black server, where as the status parameters could be modified directly on the device using a web user interface.

**Example** :
If you are running this web device named as radiotest on a localhost, then the device parameters could be viewed on URL : http://localhost:8082/view/radiotest

The device status paramters could be modified on URL : http://localhost:8082/edit/radiotest

**Note** : The name of the example device [radiotest] is same as the device configured in Red-Black.

## Driver
For operating the isoderadio device (ex: radiotest) and modifying its parameters, the Red-Black server interfaces with a driver which sends commands to the device and receives the status of various device parameters before sending it to the Red-Black server.

### Compiling the Golang based web-device

Install Go
```
https://golang.org/doc/install
```

Go module https://github.com/julienschmidt/httprouter needs to be installed locally.
```bash
$ go get github.com/julienschmidt/httprouter
```

```bash
$ go mod init HTTP_ROUTER
```

#### Compile the web-device
```bash
$ go build web_device.go
```

#### Run the web-device

```bash
$ ./web_device
```

#### Connect to the web-device

```bash
Browser URL : http://localhost:8082/view/radiotest
```

#### Fetch the status of the web-device parameters using CLI
Example:
```bash

$ curl -X GET "http://localhost:8082/device/radiotest/ref"
{"Alert":"INFO","DeviceTypeHash":"#ABCDE","RunningSince":"1m11.795971925s","StartTime":"2021-06-03 17:34:40","Status":"Enabled","UniqueId":"1232","Version":"1.0"}

$ curl -X GET "http://localhost:8082/device/radiotest/status"
{"PowerSupplyConsumption":"300","PowerSupplyVoltage":"20","SignalLevel":"500","Temperature":"40","VSWR":"10"}

$ curl -X GET "http://localhost:8082/device/radiotest"
{"VSWR":"10","PowerSupplyVoltage":"20","PowerSupplyConsumption":"300","Temperature":"40","SignalLevel":"500","Frequency":"11015","TransmissionPower":"7528","Modem":"","Antenna":"","DeviceType":"radio","Status":"Enabled","StartTime":"2021-06-03 17:34:40","RunningSince":"1m34.998094147s","Version":"1.0","Alert":"INFO","DeviceTypeHash":"#ABCDE","UniqueId":"1232","DeviceDescription":""}

$ curl -X GET "http://localhost:8082/device/radiotest/param/signallevel"
"5"

$ curl -X GET "http://localhost:8082/device/radiotest/param/vswr"
"50"
```

#### Set the control parameters of the web-device
Example:
```bash
$ $ curl --header "Content-Type: application/json" -X POST "http://localhost:8082/device/radiotest/control" --data '{"Frequency":"26000","TransmissionPower":"8000", "Modem":"Audio", "Antenna":"RF"}'
```

#### Compiling C++ driver

Download and install cmake (minimum version 3.10)
https://cmake.org/download/

Install Visual Studio 2015 Update 3

Note: The web-device driver needs Boost 1.74.0 or a higher version.
Download Link : https://www.boost.org/users/history/version_1_76_0.html

Build and install boost

Unix

https://www.boost.org/doc/libs/1_76_0/more/getting_started/unix-variants.html#easy-build-and-install

Windows

https://www.boost.org/doc/libs/1_74_0/more/getting_started/windows.html#simplified-build-from-source


Running cmake

Unix
Run the below commands inside the driver directory.
```bash
driver$ cmake .
driver$ make
```

Windows
```bash
driver> cmake .
Build the driver project (driver.sln) using VS Studio 2015 Update 3
```