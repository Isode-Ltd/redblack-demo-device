

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
