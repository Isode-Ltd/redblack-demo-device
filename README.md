

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

$ curl -X GET "http://localhost:8080/device/radio"
{"device":"radio","vswr":"1","powersupplyvoltage":"2","powersupplyconsumption":"3","temperature":"4","signallevel":"5"}

$ curl -X GET "http://localhost:8080/device/radio/param/signallevel"
"5"

$ curl -X GET "http://localhost:8080/device/radio/param/vswr"
"1"
```

#### Set the control parameters of the device
Example:
```bash
$ curl --header "Content-Type: application/json" -X POST "http://localhost:8080/device/radio/control" --data '{"frequency":"15","transmissionpower":"100", "modem":"Audio", "antenna":"RF"}'
```
View the updated parameters
```bash
Browser URL : http://localhost:8080/view/radio
```

#### Rudimentary Boost::ASIO based HTTP Get and Post clients
Compile the cpp files with Boost lib 1.76.0

For fetching the device parameters
```bash

$ g++ get-radio-driver-http.cpp -lpthread -o get

$ ./get localhost 8080 /device/radio
HTTP/1.1 200 OK
Date: Wed, 12 May 2021 11:16:00 GMT
Content-Length: 213
Content-Type: text/plain; charset=utf-8

{"devicetype":"radio","vswr":"10","powersupplyvoltage":"21","powersupplyconsumption":"31","temperature":"44","signallevel":"51","frequency":"100","transmissionpower":"100000000000","modem":"Audio","antenna":"RF"}
```

For setting the device parameters
```bash
$ g++ post-radio-driver-http.cpp -lpthread -o post
```
Note : Currently the parameters are hard-coded in the file. This would change later as things progress

```
$ ./post localhost 8080 /device/radio/control
JSON Message : {"frequency":"100","modem":"Audio"}
HTTP/1.1 200 OK
Date: Wed, 12 May 2021 11:23:44 GMT
Content-Length: 28
Content-Type: text/plain; charset=utf-8

Device parameters updated !
```

Read CBOR and printing it.
```
$ g++ driver.cpp cbor11.cpp -o driver
$ build/sampledrivers/dummygodev "Radio1" "schema/isode-radio.xml" "schema/stdparams.xml" | driver /workspace/red_black/rb_device/driver/isode-radio.xml
```

Sample output
```
Waiting to receive data....

Printing data....
24(h'78823c5374617475733e3c4465766963653e526164696f313c2f4465766963653e3c446576696365547970653e49736f6465526164696f3c2f446576696365547970653e3c506172616d3e4865617274626561743c2f506172616d3e3c4461746554696d653e313632313238333537323c2f4461746554696d653e3c2f5374617475733e')
Decoded CBOR : �X�x�<Status><Device>Radio1</Device><DeviceType>IsodeRadio</DeviceType><Param>Heartbeat</Param><DateTime>1621283572</DateTime></Status>

Waiting to receive data....

Printing data....
24(h'78893c5374617475733e3c4465766963653e526164696f313c2f4465766963653e3c446576696365547970653e49736f6465526164696f3c2f446576696365547970653e3c506172616d3e506f776572537570706c79436f6e73756d7074696f6e3c2f506172616d3e3c496e74656765723e37313338393c2f496e74656765723e3c2f5374617475733e0a')
Decoded CBOR : �X�x�<Status><Device>Radio1</Device><DeviceType>IsodeRadio</DeviceType><Param>PowerSupplyConsumption</Param><Integer>71389</Integer></Status>

Waiting to receive data....

Printing data....
24(h'78893c5374617475733e3c4465766963653e526164696f313c2f4465766963653e3c446576696365547970653e49736f6465526164696f3c2f446576696365547970653e3c506172616d3e4d6f6e69746f72696e6753696e63653c2f506172616d3e3c4461746554696d653e313632313238333534323c2f4461746554696d653e3c2f5374617475733e0a')
Decoded CBOR : �X�x�<Status><Device>Radio1</Device><DeviceType>IsodeRadio</DeviceType><Param>MonitoringSince</Param><DateTime>1621283542</DateTime></Status>

Waiting to receive data....

Printing data....
24(h'78973c5374617475733e3c4465766963653e526164696f313c2f4465766963653e3c446576696365547970653e49736f6465526164696f3c2f446576696365547970653e3c506172616d3e56657273696f6e3c2f506172616d3e3c537472696e673e536f6d6520737472696e672076616c75652072656c6174656420746f2056657273696f6e3c2f537472696e673e3c2f5374617475733e0a')
Decoded CBOR : �X�x�<Status><Device>Radio1</Device><DeviceType>IsodeRadio</DeviceType><Param>Version</Param><String>Some string value related to Version</String></Status>

Waiting to receive data....
```
