

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
$ g++ driver.cpp cbor11.cpp -o driver -lpthread
/workspace/source_red_black/redblack$ build/sampledrivers/dummygodev "Radio1" "schema/isode-radio.xml" "schema/stdparams.xml" | /workspace/red_black/rb_device/driver/driver /workspace/red_black/rb_device/driver/isode-radio.xml localhost 8080
```

Sample output
```

Waiting to receive data....

Printing data....
24(h'78893c5374617475733e3c4465766963653e526164696f313c2f4465766963653e3c446576696365547970653e49736f6465526164696f3c2f446576696365547970653e3c506172616d3e506f776572537570706c79436f6e73756d7074696f6e3c2f506172616d3e3c496e74656765723e37323436313c2f496e74656765723e3c2f5374617475733e0a')

Decoded CBOR : �X�x�<Status><Device>Radio1</Device><DeviceType>IsodeRadio</DeviceType><Param>PowerSupplyConsumption</Param><Integer>72461</Integer></Status>

Device status param [PowerSupplyConsumption] received. Will issue HTTP GET
HTTP Get request to [ Device Host : localhost, Device Port : 8080, Device Target /device/radio/param/powersupplyconsumption]
HTTP/1.1 200 OK
Date: Thu, 20 May 2021 11:24:06 GMT
Content-Length: 5
Content-Type: text/plain; charset=utf-8

"31"

Response to HTTP Get ["31"]
================= Success ==================

Waiting to receive data....

Printing data....
24(h'787c3c5374617475733e3c4465766963653e526164696f313c2f4465766963653e3c446576696365547970653e49736f6465526164696f3c2f446576696365547970653e3c506172616d3e54656d70657261747572653c2f506172616d3e3c496e74656765723e3133303c2f496e74656765723e3c2f5374617475733e0a')

Decoded CBOR : �X~x|<Status><Device>Radio1</Device><DeviceType>IsodeRadio</DeviceType><Param>Temperature</Param><Integer>130</Integer></Status>

Device status param [Temperature] received. Will issue HTTP GET
HTTP Get request to [ Device Host : localhost, Device Port : 8080, Device Target /device/radio/param/temperature]
HTTP/1.1 200 OK
Date: Thu, 20 May 2021 11:24:06 GMT
Content-Length: 5
Content-Type: text/plain; charset=utf-8

"44"

Response to HTTP Get ["44"]
================= Success ==================

```
