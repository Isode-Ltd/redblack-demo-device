

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

$ curl -X GET "http://localhost:8080/device/radio/param/SignalLevel"
"5"

$ curl -X GET "http://localhost:8080/device/radio/param/VSWR"
"1"
```
