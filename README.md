## ISODE's Red/Black
Isode’s Red/Black product provides a flexible management capability to manage a wide range of devices.

Refer : https://www.isode.com/products/red-black.html

Every supported device needs an XML Abstract Device Specification and a device driver for the device. This project provides a mock radio device with a Web interface (coded in Go), an XML Abstract Device Specification, and a Red/Black driver (coded in C++). These can be compiled and used with Isode’s Red/Black product.

The intent is to facilitate writing device drivers, by providing a concrete example that device driver writers can use as a starting point. The source code can be downloaded and compiled.

All of this code is provided with a simple Open Source license, so that the code can be used as the basis for writing a real device driver.

## Device
This repository contains a go program (called 'isode-device-web-manager') that manages isode radio dummy devices. Just like an actual isode radio, the dummy web radio device has control and status parameters. The control parameters could be modified using a Red-Black server, where as the status parameters could be modified directly on the device using a web interface provided by the isode-device-web-manager.

**Example** :
If you are running the isode-device-web-manager on a localhost, it can create a dummy isode radio devices (example radiotest) by accessing the URL : http://localhost:8082/view/radiotest

The status paramters of the dummy isode radio device (ex: radiotest) could be modified on URL : http://localhost:8082/edit/radiotest

**Note** : The name of the example isode dummy radio device (radiotest) is same as the device name configured in Red-Black.

## Driver
For operating the dummy radio web device (ex: radiotest) and modifying its parameters, the Red-Black server communicates with a driver which sends commands to the isode-device-web-manager and receives the status of various device parameters before sending it to the Red-Black server.

**Note** : The files _stdparams.xml_ and _isode-radio.xml_ are used for communication between the Red/Black server and the sample device. These files, as well as other device-specific XML files, are shipped as part of the Red/Black installation and are included in this project for reference. If you use this project as a template for your own device driver, then you can create a new device XML file based on isode-radio.xml (e.g. my-own-driver.xml) and install that in the location where the Red/Black server can find it.

### Compiling the Golang based isode-device-web-manager

Install Go (1.16)
```
https://golang.org/doc/install
```

Run the following commands in the device folder.

```bash
device$ go get
```

#### Compile the isode-device-web-manager in the device folder

```bash
device$ go build isode-device-web-manager.go
```

#### Run the isode-device-web-manager in the device folder

Note: The isode-device-web-manager binary, view.html and edit.html template files should be present in the same directory.

```bash
device$ ./isode-device-web-manager
```

#### Connect to the isode-device-web-manager's sample device radiotest.

```bash
Browser URL : http://localhost:8082/view/radiotest
```

#### Fetch the status of the isode-device-web-manager's device (radiotest) parameters using CLI

Note : In the below example [radiotest] is a dummy isode radio device managed by isode-device-web-manager. This radio device could have any name.

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

#### Set the control parameters of the dummy isode web device
Example:
```bash
$ $ curl --header "Content-Type: application/json" -X POST "http://localhost:8082/device/radiotest/control" --data '{"Frequency":"26000","TransmissionPower":"8000", "Modem":"Audio", "Antenna":"RF"}'
```

### Compiling C++ driver

Download and install cmake (minimum version 3.10)
https://cmake.org/download/

**On Windows**
Install Visual Studio Professional 2015 With Update 3
Install Microsoft Visual Build Tools for Visual Studio 2015 Update 3

**Note:** The web-device driver needs Boost 1.74.0 or a higher version.

**Download Link** : https://www.boost.org/users/history/version_1_76_0.html

**Build and install boost**

Unix

https://www.boost.org/doc/libs/1_76_0/more/getting_started/unix-variants.html#easy-build-and-install

Windows

https://www.boost.org/doc/libs/1_76_0/more/getting_started/windows.html#simplified-build-from-source

For example on Windows:
```bash
c:\boost_1_76_0>bootstrap
c:\boost_1_76_0>b2 -j8 toolset=msvc address-model=64 architecture=x86 link=static threading=multi runtime-link=shared --build-type=complete stage install --prefix="C:\boost"
```

**Running cmake**
* After doing a Boost build and install, use CMake / CMake GUI to configure the driver.

**Unix**
* Run the below commands inside the driver directory.
```bash
driver$ cmake .
driver$ make
```

**Windows**

Using CMake from the a command prompt
* Ensure that the environment variable BOOST_ROOT points at your Boost installation
* Run cmake to generate a Visual Studio solution file for the platform (x64) you need
* Invoke Visual Studio to build the solution

For example

```bash
C:\driver> set BOOST_ROOT c:\boost
C:\driver> cmake -G "Visual Studio 14 2015 Win64"
C:\driver> msbuild.exe isode-demo-radio-driver.sln /property:Configuration=Release
```

In CMake GUI
* Specify the source location of driver directory and the location to build the binaries.
* Set the generator for driver to Visual Studio 14 2015.
* Choose appropriate optional platform for generator (x64).
* In the CMake environment variables, you might need to specify BOOST_ROOT pointing to the location of the boost installation directory ("c:\boost" in the example above).

After successfully configuring and generating the driver solution using CMake, do
* Open the solution file (isode-radio-demo-driver.sln) using VS Studio 2015 Update 3 IDE and build the binary.
* Or build from the command line using a command of the form:

Example
```bash
C:\driver> msbuild.exe c:\driver\isode-demo-radio-driver.sln /property:Configuration=Release
```

### Configuring and monitoring the device(s) in Red/Black

Refer to the Red/Black admin guide for the installation and setup of the Red/Black server.

After installing and starting the Red/Black server, connect to it with a web browser (for example, https://localhost:8080). After you have authenticated, use the "Configuration" section and choose the "Device List" option to add a new Sample Radio device.

Below is the configuration of the sample radio device configured in Red/Black server on **Linux**.

**Device Name**: radiotest ( Note : This matches with the device URL [http://localhost:8082/view/radiotest] )<br>
**Template**: IsodeRadio:Basic Radio with mock driver. ( Selected from the drop down )<br>
**Driver Options**: Custom driver ( Selected from the drop down )<br>
**Driver**: isode-demo-radio-driver ( The driver binary that is generated after driver compilation )<br>
**Additional arguments**: `--host localhost --port 8082 --device_name radiotest --schema_file /opt/isode/redblack/share/redblack/schema/isode-radio.xml --std_params_file /opt/isode/redblack/share/redblack/stdparams.xml` ( These arguments are needed by the sample demo driver written for the radio device. The path arguments might differ for your particular Red/Black installation. )

Below is the configuration of the sample radio device configured in Red/Black server on **Windows**.

**Device Name**: radiotest ( Note : This matches with the device URL [http://localhost:8082/view/radiotest] )<br>
**Template**: IsodeRadio:Basic Radio with mock driver. ( Selected from the drop down )<br>
**Driver Options**: Custom driver ( Selected from the drop down )<br>
**Driver**: isode-demo-radio-driver ( The driver binary that is generated after driver compilation )<br>
**Additional arguments**: `--host localhost --port 8082 --device_name radiotest --schema_file "C:\Program Files\Isode RedBlack 1.0v7\share\redblack\schema\isode-radio.xml" --std_params_file "C:\Program Files\Isode RedBlack 1.0v7\share\redblack\stdparams.xml"` ( These arguments are needed by the sample demo driver written for the radio device. The path arguments might differ for your particular Red/Black installation. )

After you have configured the Sample Radio device, you can monitor it by visiting e.g. https://localhost:8080/monitor in a web browser.