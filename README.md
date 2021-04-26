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
```bash
$ curl -X get "http://localhost:8080/status"
```
