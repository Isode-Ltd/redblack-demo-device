package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"html/template"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"regexp"
	"strconv"
	"time"

	"github.com/julienschmidt/httprouter"
)

type Device struct {

	// Status parameters
	VSWR                   string `json:"VSWR"`
	PowerSupplyVoltage     string `json:"PowerSupplyVoltage"`
	PowerSupplyConsumption string `json:"PowerSupplyConsumption"`
	Temperature            string `json:"Temperature"`
	SignalLevel            string `json:"SignalLevel"`

	// Control parameters
	Frequency         string `json:"Frequency"`
	TransmissionPower string `json:"TransmissionPower"`

	// Referenced control parameters
	Enabled string `json:"Enabled"`

	// Referenced status parameters
	DeviceType     string `json:"DeviceType"`
	Status         string `json:"Status"`
	StartTime      string `json:"StartTime"`
	Version        string `json:"Version"`
	DeviceTypeHash string `json:"DeviceTypeHash"`
	UniqueID       string `json:"UniqueID"`

	// Alert parameters
	Alert        string `json:"Alert"`
	AlertMessage string `json:"AlertMessage"`
}

func check(e error) {
	if e != nil {
		panic(e)
	}
}

func (p *Device) CorrectParams() *Device {

	_, err := strconv.Atoi(p.VSWR)

	if err != nil {
		p.VSWR = "50"
	}

	_, err = strconv.Atoi(p.PowerSupplyVoltage)
	if err != nil {
		p.PowerSupplyVoltage = "200"
	}

	_, err = strconv.Atoi(p.PowerSupplyConsumption)
	if err != nil {
		p.PowerSupplyConsumption = "50000"
	}

	_, err = strconv.Atoi(p.Temperature)
	if err != nil {
		p.Temperature = "100"
	}

	_, err = strconv.Atoi(p.SignalLevel)
	if err != nil {
		p.SignalLevel = "5"
	}

	if p.Alert != "Info" && p.Alert != "Warning" &&
		p.Alert != "Error" && p.Alert != "Severe" &&
		p.Alert != "Crirical" {
		p.Alert = "Info"
	}

	return p
}

func (p *Device) CheckLimit() bool {

	// If the device if powered off, no need to check the limit.
	// Just return true.
	if p.Status == "OFFLINE" {
		return true
	}

	VSWR, _ := strconv.Atoi(p.VSWR)
	if VSWR < 10 || VSWR > 100 {
		return false
	}

	PowerSupplyVoltage, _ := strconv.Atoi(p.PowerSupplyVoltage)
	if PowerSupplyVoltage < 100 || PowerSupplyVoltage > 400 {
		return false
	}

	PowerSupplyConsumption, _ := strconv.Atoi(p.PowerSupplyConsumption)
	if PowerSupplyConsumption < 0 || PowerSupplyConsumption > 100000 {
		return false
	}

	Temperature, _ := strconv.Atoi(p.Temperature)
	if Temperature < -20 || Temperature > 200 {
		return false
	}

	SignalLevel, _ := strconv.Atoi(p.SignalLevel)
	if SignalLevel < -40 || SignalLevel > 15 {
		return false
	}

	return true
}

func (p *Device) save() error {

	fmt.Printf("%+v\n", p)

	filename := p.DeviceType + ".status"

	f, err := os.Create(filename)
	check(err)
	defer f.Close()

	// Device Status Params
	VSWR := p.VSWR
	PowerSupplyVoltage := p.PowerSupplyVoltage
	PowerSupplyConsumption := p.PowerSupplyConsumption
	Temperature := p.Temperature
	SignalLevel := p.SignalLevel

	f.WriteString("[VSWR][" + VSWR + "]\n")
	f.WriteString("[PowerSupplyVoltage][" + PowerSupplyVoltage + "]\n")
	f.WriteString("[PowerSupplyConsumption][" + PowerSupplyConsumption + "]\n")
	f.WriteString("[Temperature][" + Temperature + "]\n")
	f.WriteString("[SignalLevel][" + SignalLevel + "]\n")

	// Referenced Status Params
	DeviceType := p.DeviceType
	Status := p.Status
	StartTime := p.StartTime
	Version := p.Version
	DeviceTypeHash := p.DeviceTypeHash
	UniqueID := p.UniqueID

	f.WriteString("[DeviceType][" + DeviceType + "]\n")
	f.WriteString("[Status][" + Status + "]\n")
	f.WriteString("[StartTime][" + StartTime + "]\n")
	f.WriteString("[Version][" + Version + "]\n")
	f.WriteString("[DeviceTypeHash][" + DeviceTypeHash + "]\n")
	f.WriteString("[UniqueID][" + UniqueID + "]\n")

	// Alert params
	filename = p.DeviceType + ".alert"

	f, err = os.Create(filename)
	check(err)
	defer f.Close()

	Alert := p.Alert
	AlertMessage := p.AlertMessage

	f.WriteString("[Alert][" + Alert + "]\n")
	f.WriteString("[AlertMessage][" + AlertMessage + "]\n")

	return err
}

func LoadDeviceInfo(devicetype string) (*Device, error) {

	// Read the status parameters of the device
	filename := devicetype + ".status"
	status_file, err := os.Open(filename)

	if err != nil {
		status_file, err = os.Create(filename)
	}

	check(err)
	defer status_file.Close()

	scanner := bufio.NewScanner(status_file)

	var line string
	var val_VSWR, val_PowerSupplyVoltage, val_PowerSupplyConsumption string
	var val_Temperature, val_SignalLevel string
	var val_Status, val_StartTime string
	var val_Version, val_UniqueID, val_DeviceTypeHash string

	val_StartTime = start_time_.Format("2006-01-02 15:04:05")

	for scanner.Scan() {
		line = scanner.Text()
		exp := regexp.MustCompile(`\[(.*?)\]\[(.*?)\]`)

		if line != "" {
			match := exp.FindStringSubmatch(line)
			if match[1] == "VSWR" {
				val_VSWR = match[2]
			}
			if match[1] == "PowerSupplyVoltage" {
				val_PowerSupplyVoltage = match[2]
			}
			if match[1] == "PowerSupplyConsumption" {
				val_PowerSupplyConsumption = match[2]
			}
			if match[1] == "Temperature" {
				val_Temperature = match[2]
			}
			if match[1] == "SignalLevel" {
				val_SignalLevel = match[2]
			}
			if match[1] == "Status" {
				val_Status = match[2]
			}
			if match[1] == "Version" {
				val_Version = match[2]
			}
			if match[1] == "DeviceTypeHash" {
				val_DeviceTypeHash = match[2]
			}
			if match[1] == "UniqueID" {
				val_UniqueID = match[2]
			}
		}
	}

	// Read the contorl parameters of the device
	filename = devicetype + ".control"
	control_file, err := os.Open(filename)

	if err != nil {
		control_file, err = os.Create(filename)
	}

	check(err)
	defer control_file.Close()

	scanner = bufio.NewScanner(control_file)
	var val_Enabled, val_Frequency, val_TransmissionPower string

	for scanner.Scan() {
		line = scanner.Text()
		exp := regexp.MustCompile(`\[(.*?)\]\[(.*?)\]`)

		if line != "" {
			match := exp.FindStringSubmatch(line)
			if match[1] == "Frequency" {
				val_Frequency = match[2]
			}
			if match[1] == "TransmissionPower" {
				val_TransmissionPower = match[2]
			}
			if match[1] == "Enabled" {
				val_Enabled = match[2]
			}
		}
	}

	// Read the contorl parameters of the device
	filename = devicetype + ".alert"
	alert_file, err := os.Open(filename)

	if err != nil {
		alert_file, err = os.Create(filename)
	}

	check(err)
	defer alert_file.Close()

	scanner = bufio.NewScanner(alert_file)
	var val_Alert, val_AlertMessage string

	for scanner.Scan() {
		line = scanner.Text()
		exp := regexp.MustCompile(`\[(.*?)\]\[(.*?)\]`)

		if line != "" {
			match := exp.FindStringSubmatch(line)
			if match[1] == "Alert" {
				val_Alert = match[2]
			}
			if match[1] == "AlertMessage" {
				val_AlertMessage = match[2]
			}
		}
	}

	// Set the default values for device status params.
	if val_VSWR == "" {
		val_VSWR = "50"
	}
	if val_PowerSupplyVoltage == "" {
		val_PowerSupplyVoltage = "200"
	}
	if val_PowerSupplyConsumption == "" {
		val_PowerSupplyConsumption = "50000"
	}
	if val_Temperature == "" {
		val_Temperature = "100"
	}
	if val_SignalLevel == "" {
		val_SignalLevel = "5"
	}

	// Set the default values for device control params.
	if val_Frequency == "" {
		val_Frequency = "12000"
	}
	if val_TransmissionPower == "" {
		val_TransmissionPower = "10000"
	}
	if val_Enabled == "" {
		val_Enabled = "true"
	}

	// Set the default values for referenced status params.
	if val_DeviceTypeHash == "" {
		val_DeviceTypeHash = "#ISODERADIO"
	}
	if val_UniqueID == "" {
		val_UniqueID = "SAMPLE_RADIO_1"
	}
	if val_Version == "" {
		val_Version = "1.0"
	}
	if val_Status == "" {
		val_Status = "Operational"
	}

	// Set the default values for alert.
	if val_Alert == "" {
		val_Alert = "Info"
	}
	if val_AlertMessage == "" {
		val_AlertMessage = "Parameters within limit."
	}

	return &Device{
		DeviceType:             devicetype,
		VSWR:                   val_VSWR,
		PowerSupplyVoltage:     val_PowerSupplyVoltage,
		PowerSupplyConsumption: val_PowerSupplyConsumption,
		Temperature:            val_Temperature,
		SignalLevel:            val_SignalLevel,
		Frequency:              val_Frequency,
		TransmissionPower:      val_TransmissionPower,
		Enabled:                val_Enabled,
		Status:                 val_Status,
		StartTime:              val_StartTime,
		Version:                val_Version,
		Alert:                  val_Alert,
		AlertMessage:           val_AlertMessage,
		DeviceTypeHash:         val_DeviceTypeHash,
		UniqueID:               val_UniqueID}, nil
}

func ViewHandler(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	device_info, err := LoadDeviceInfo(ps.ByName("device"))
	if err != nil {
		http.Redirect(w, r, "/edit/"+ps.ByName("device"), http.StatusFound)
		return
	}
	RenderTemplate(w, "view", device_info)
}

func EditHandler(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	device_info, err := LoadDeviceInfo(ps.ByName("device"))
	if err != nil {
		device_info = &Device{DeviceType: ps.ByName("device")}
	}
	RenderTemplate(w, "edit", device_info)
}

func SaveHandler(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	val_VSWR := r.FormValue("VSWR")
	val_PowerSupplyVoltage := r.FormValue("PowerSupplyVoltage")
	val_PowerSupplyConsumption := r.FormValue("PowerSupplyConsumption")
	val_Temperature := r.FormValue("Temperature")
	val_SignalLevel := r.FormValue("SignalLevel")
	val_Status := r.FormValue("Status")
	val_StartTime := r.FormValue("StartTime")
	val_Version := r.FormValue("Version")
	val_DeviceTypeHash := r.FormValue("DeviceTypeHash")
	val_UniqueID := r.FormValue("UniqueID")
	val_Alert := r.FormValue("Alert")
	val_AlertMessage := r.FormValue("AlertMessage")

	p := &Device{
		DeviceType:             ps.ByName("device"),
		VSWR:                   val_VSWR,
		PowerSupplyVoltage:     val_PowerSupplyVoltage,
		PowerSupplyConsumption: val_PowerSupplyConsumption,
		Temperature:            val_Temperature,
		SignalLevel:            val_SignalLevel,
		Status:                 val_Status,
		StartTime:              val_StartTime,
		Version:                val_Version,
		DeviceTypeHash:         val_DeviceTypeHash,
		UniqueID:               val_UniqueID,
		Alert:                  val_Alert,
		AlertMessage:           val_AlertMessage,
	}

	// If the params are invalid reset their values to default
	p = p.CorrectParams()

	// Check if the params are within limit, if not indicate the same in the alert message
	if !p.CheckLimit() {
		if p.Alert == "Info" {
			p.Alert = "Warning"
		}
		p.AlertMessage = "Parameters beyond range."
	} else {
		p.Alert = "Info"
		p.AlertMessage = "Parameters within range."
	}

	err := p.save()
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	http.Redirect(w, r, "/view/"+ps.ByName("device"), http.StatusFound)
}

var templates = template.Must(template.ParseFiles("edit.html", "view.html"))

func RenderTemplate(w http.ResponseWriter, tmpl string,
	params *Device) {
	err := templates.ExecuteTemplate(w, tmpl+".html", params)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
	}
}

var validPath = regexp.MustCompile("^/(edit|save|view)/([a-zA-Z0-9]+)$")

func MakeHandler(fn func(http.ResponseWriter, *http.Request, string)) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		m := validPath.FindStringSubmatch(r.URL.Path)
		if m == nil {
			http.NotFound(w, r)
			return
		}
		fn(w, r, m[2])
	}
}

func GetAllDeviceParams(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	device_info, err := LoadDeviceInfo(ps.ByName("device"))
	if err == nil {
		json.NewEncoder(w).Encode(device_info)
	}
}

func PowerOff(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	var val_VSWR, val_PowerSupplyVoltage, val_PowerSupplyConsumption string
	var val_Temperature, val_SignalLevel string
	var val_Status, val_StartTime string
	var val_DeviceType string
	var val_Version, val_Alert, val_AlertMessage, val_UniqueID, val_DeviceTypeHash string
	var val_Enabled, val_Frequency, val_TransmissionPower string

	//var control_params map[string]string
	fmt.Println("\n=============== Message from driver ===================")
	fmt.Println("Turn-Off Requested")
	fmt.Println("================= End of Message ====================")

	val_DeviceType = ps.ByName("device")
	// Set the default values for device status params.
	val_VSWR = "00"
	val_PowerSupplyVoltage = "000"
	val_PowerSupplyConsumption = "00000"
	val_Temperature = "000"
	val_SignalLevel = "0"
	// Set the default values for device control params.
	val_Frequency = "00000"
	val_TransmissionPower = "00000"
	val_Enabled = "true"
	// Set the default values for referenced status params.
	val_DeviceTypeHash = "#ISODERADIO"
	val_UniqueID = "SAMPLE_RADIO_1"
	val_Version = "1.0"
	val_Status = "Not Operational"
	val_Alert = "NONE"
	val_AlertMessage = "NONE"

	filename := val_DeviceType + ".status"
	status_file, err := os.Create(filename)
	check(err)
	defer status_file.Close()

	status_file.WriteString("[VSWR][" + val_VSWR + "]\n")
	status_file.WriteString("[PowerSupplyVoltage][" + val_PowerSupplyVoltage + "]\n")
	status_file.WriteString("[PowerSupplyConsumption][" + val_PowerSupplyConsumption + "]\n")
	status_file.WriteString("[Temperature][" + val_Temperature + "]\n")
	status_file.WriteString("[SignalLevel][" + val_SignalLevel + "]\n")
	status_file.WriteString("[DeviceType][" + val_DeviceType + "]\n")
	status_file.WriteString("[Status][" + val_Status + "]\n")
	status_file.WriteString("[StartTime][" + val_StartTime + "]\n")
	status_file.WriteString("[Version][" + val_Version + "]\n")
	status_file.WriteString("[Alert][" + val_Alert + "]\n")
	status_file.WriteString("[AlertMessage][" + val_AlertMessage + "]\n")
	status_file.WriteString("[DeviceTypeHash][" + val_DeviceTypeHash + "]\n")
	status_file.WriteString("[UniqueID][" + val_UniqueID + "]\n")

	filename = val_DeviceType + ".control"
	control_file, err := os.Create(filename)
	check(err)
	defer control_file.Close()

	control_file.WriteString("[Frequency][" + val_Frequency + "]\n")
	control_file.WriteString("[TransmissionPower][" + val_TransmissionPower + "]\n")
	control_file.WriteString("[Enabled][" + val_Enabled + "]\n")

	device_info, err := LoadDeviceInfo(ps.ByName("device"))
	if err == nil {
		json.NewEncoder(w).Encode(device_info)
	}
}

func ResetDevice(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	var val_VSWR, val_PowerSupplyVoltage, val_PowerSupplyConsumption string
	var val_Temperature, val_SignalLevel string
	var val_Status, val_StartTime string
	var val_DeviceType string
	var val_Version, val_Alert, val_AlertMessage, val_UniqueID, val_DeviceTypeHash string
	var val_Enabled, val_Frequency, val_TransmissionPower string

	//var control_params map[string]string
	fmt.Println("\n=============== Message from driver ===================")
	fmt.Println("Reset Requested")
	fmt.Println("================= End of Message ====================")

	val_DeviceType = ps.ByName("device")
	// Set the default values for device status params.
	val_VSWR = "50"
	val_PowerSupplyVoltage = "200"
	val_PowerSupplyConsumption = "50000"
	val_Temperature = "100"
	val_SignalLevel = "5"
	// Set the default values for device control params.
	val_Frequency = "12000"
	val_TransmissionPower = "10000"
	val_Enabled = "true"
	// Set the default values for referenced status params.
	val_DeviceTypeHash = "#ISODERADIO"
	val_UniqueID = "SAMPLE_RADIO_1"
	val_Version = "1.0"
	val_Status = "Operational"
	val_Alert = "Info"
	val_AlertMessage = "Parameters within limit."

	filename := val_DeviceType + ".status"
	status_file, err := os.Create(filename)
	check(err)
	defer status_file.Close()

	status_file.WriteString("[VSWR][" + val_VSWR + "]\n")
	status_file.WriteString("[PowerSupplyVoltage][" + val_PowerSupplyVoltage + "]\n")
	status_file.WriteString("[PowerSupplyConsumption][" + val_PowerSupplyConsumption + "]\n")
	status_file.WriteString("[Temperature][" + val_Temperature + "]\n")
	status_file.WriteString("[SignalLevel][" + val_SignalLevel + "]\n")
	status_file.WriteString("[DeviceType][" + val_DeviceType + "]\n")
	status_file.WriteString("[Status][" + val_Status + "]\n")
	status_file.WriteString("[StartTime][" + val_StartTime + "]\n")
	status_file.WriteString("[Version][" + val_Version + "]\n")
	status_file.WriteString("[DeviceTypeHash][" + val_DeviceTypeHash + "]\n")
	status_file.WriteString("[UniqueID][" + val_UniqueID + "]\n")

	filename = val_DeviceType + ".control"
	control_file, err := os.Create(filename)
	check(err)
	defer control_file.Close()

	control_file.WriteString("[Frequency][" + val_Frequency + "]\n")
	control_file.WriteString("[TransmissionPower][" + val_TransmissionPower + "]\n")
	control_file.WriteString("[Enabled][" + val_Enabled + "]\n")

	filename = val_DeviceType + ".alert"
	alert_file, err := os.Create(filename)
	check(err)
	defer alert_file.Close()

	control_file.WriteString("[Alert][" + val_Alert + "]\n")
	control_file.WriteString("[AlertMessage][" + val_AlertMessage + "]\n")

	device_info, err := LoadDeviceInfo(ps.ByName("device"))
	if err == nil {
		json.NewEncoder(w).Encode(device_info)
	}
}

func GetParam(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	device_info, err := LoadDeviceInfo(ps.ByName("device"))
	param := ps.ByName("param")

	if err == nil {
		if param == "VSWR" {
			json.NewEncoder(w).Encode(device_info.VSWR)
		} else if param == "PowerSupplyVoltage" {
			json.NewEncoder(w).Encode(device_info.PowerSupplyVoltage)
		} else if param == "PowerSupplyConsumption" {
			json.NewEncoder(w).Encode(device_info.PowerSupplyConsumption)
		} else if param == "SignalLevel" {
			json.NewEncoder(w).Encode(device_info.SignalLevel)
		} else if param == "Temperature" {
			json.NewEncoder(w).Encode(device_info.Temperature)
		} else if param == "Alert" {
			json.NewEncoder(w).Encode(device_info.Alert)
		} else if param == "AlertMessage" {
			json.NewEncoder(w).Encode(device_info.AlertMessage)
		}
	}
}

func GetAllDeviceStatus(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	device_info, err := LoadDeviceInfo(ps.ByName("device"))
	map_status := map[string]string{"VSWR": device_info.VSWR,
		"PowerSupplyVoltage":     device_info.PowerSupplyVoltage,
		"PowerSupplyConsumption": device_info.PowerSupplyConsumption,
		"Temperature":            device_info.Temperature,
		"SignalLevel":            device_info.SignalLevel}
	if err == nil {
		json.NewEncoder(w).Encode(map_status)
	}
}

func GetAllRefParams(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	device_info, err := LoadDeviceInfo(ps.ByName("device"))
	map_status := map[string]string{"Status": device_info.Status,
		"Version":        device_info.Version,
		"DeviceTypeHash": device_info.DeviceTypeHash,
		"UniqueID":       device_info.UniqueID,
		"StartTime":      device_info.StartTime,
		"Alert":          device_info.Alert,
		"AlertMessage":   device_info.AlertMessage}
	if err == nil {
		json.NewEncoder(w).Encode(map_status)
	}
}

func SetControlParam(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	// Create a map of string to arbitary data type
	var control_params map[string]string
	req_body, _ := ioutil.ReadAll(r.Body)
	fmt.Println("\n=============== Message from driver ===================")
	fmt.Println(r.Body)
	fmt.Println(string(req_body))
	fmt.Println("================= End of Message ====================")
	if err := json.Unmarshal(req_body, &control_params); err != nil {
		panic(err)
	}

	device_info, _ := LoadDeviceInfo(ps.ByName("device"))

	var val_Frequency, val_TransmissionPower, val_Enabled string

	// Fetch the control parameters before overwriting the file
	val_Frequency = device_info.Frequency
	val_TransmissionPower = device_info.TransmissionPower
	val_Enabled = device_info.Enabled

	filename := device_info.DeviceType + ".control"

	f, err := os.Create(filename)
	check(err)
	defer f.Close()

	if val, ok := control_params["Frequency"]; ok {
		val_Frequency = val
	}
	if val, ok := control_params["TransmissionPower"]; ok {
		val_TransmissionPower = val
	}
	if val, ok := control_params["Enabled"]; ok {
		val_Enabled = val
	}

	f.WriteString("[Frequency][" + val_Frequency + "]\n")
	f.WriteString("[TransmissionPower][" + val_TransmissionPower + "]\n")
	f.WriteString("[Enabled][" + val_Enabled + "]\n")
	fmt.Fprint(w, "Device parameters updated !\n")

	if !device_info.CheckLimit() {
		filename := device_info.DeviceType + ".alert"
		f, err := os.Create(filename)
		check(err)
		defer f.Close()

		if device_info.Alert == "Info" {
			f.WriteString("[Alert][Warning]\n")
		}
		f.WriteString("[AlertMessage][Parameters boyond range]\n")
	}
}

func Index(w http.ResponseWriter, r *http.Request, _ httprouter.Params) {
	fmt.Fprint(w, "Welcome !\n\n")
	fmt.Fprint(w, "For viewing device parameters : http://localhost:8082/view/<device_name>\n\n")
	fmt.Fprint(w, "For updating device parameters : http://localhost:8082/edit/<device_name>\n\n")
}

var start_time_ = time.Now()

func main() {
	router := httprouter.New()
	router.GET("/", Index)
	router.GET("/view/:device", ViewHandler)
	router.GET("/edit/:device", EditHandler)
	router.GET("/save/:device", SaveHandler)
	router.GET("/device/:device/param/:param", GetParam)
	router.GET("/device/:device", GetAllDeviceParams)
	router.GET("/device/:device/status", GetAllDeviceStatus)
	router.GET("/device/:device/reset", ResetDevice)
	router.GET("/device/:device/poweroff", PowerOff)
	router.GET("/device/:device/ref", GetAllRefParams)
	router.POST("/device/:device/control", SetControlParam)
	log.Fatal(http.ListenAndServe(":8082", router))
}
