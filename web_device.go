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

	"github.com/julienschmidt/httprouter"
)

type Device struct {
	// Device Type
	DeviceType string `json:"devicetype"`
	// Status parameters
	VSWR                   string `json:"VSWR"`
	PowerSupplyVoltage     string `json:"PowerSupplyVoltage"`
	PowerSupplyConsumption string `json:"PowerSupplyConsumption"`
	Temperature            string `json:"Temperature"`
	SignalLevel            string `json:"SignalLevel"`
	// Control parameters
	Frequency         string `json:"Frequency"`
	TransmissionPower string `json:"TransmissionPower"`
	Modem             string `json:"Modem"`
	Antenna           string `json:"Antenna"`
}

func check(e error) {
	if e != nil {
		panic(e)
	}
}

func (p *Device) save() error {
	filename := p.DeviceType + ".status"

	f, err := os.Create(filename)
	check(err)
	defer f.Close()

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

	f, err = os.OpenFile(p.DeviceType+".log", os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		log.Println(err)
	}
	defer f.Close()
	if _, err := f.WriteString("text to append\n"); err != nil {
		log.Println(err)
	}
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
		}
	}

	// Read the contorl parameters of the device
	filename = devicetype + ".control"
	control_file, err := os.Open(filename)

	if err != nil {
		status_file, err = os.Create(filename)
	}

	check(err)
	defer control_file.Close()

	scanner = bufio.NewScanner(control_file)
	var val_Frequency, val_TransmissionPower, val_Modem, val_Antenna string

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
			if match[1] == "Modem" {
				val_Modem = match[2]
			}
			if match[1] == "Antenna" {
				val_Antenna = match[2]
			}
		}
	}

	return &Device{DeviceType: devicetype, VSWR: val_VSWR, PowerSupplyVoltage: val_PowerSupplyVoltage,
		PowerSupplyConsumption: val_PowerSupplyConsumption, Temperature: val_Temperature,
		SignalLevel: val_SignalLevel, Frequency: val_Frequency,
		TransmissionPower: val_TransmissionPower, Modem: val_Modem, Antenna: val_Antenna}, nil
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

	p := &Device{DeviceType: ps.ByName("device"),
		VSWR:                   val_VSWR,
		PowerSupplyVoltage:     val_PowerSupplyVoltage,
		PowerSupplyConsumption: val_PowerSupplyConsumption,
		Temperature:            val_Temperature,
		SignalLevel:            val_SignalLevel}

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

func GetParam(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	device_info, err := LoadDeviceInfo(ps.ByName("device"))
	param := ps.ByName("param")

	if err == nil {
		if param == "vswr" {
			json.NewEncoder(w).Encode(device_info.VSWR)
		} else if param == "powersupplyvoltage" {
			json.NewEncoder(w).Encode(device_info.PowerSupplyVoltage)
		} else if param == "powersupplyconsumption" {
			json.NewEncoder(w).Encode(device_info.PowerSupplyConsumption)
		} else if param == "signallevel" {
			json.NewEncoder(w).Encode(device_info.SignalLevel)
		} else if param == "temperature" {
			json.NewEncoder(w).Encode(device_info.Temperature)
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

func SetControlParam(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {
	// Create a map of string to arbitary data type
	var control_params map[string]string
	req_body, _ := ioutil.ReadAll(r.Body)
	fmt.Println("==================================")
	fmt.Println(r.Body)
	fmt.Println(string(req_body))
	fmt.Println("==================================")
	if err := json.Unmarshal(req_body, &control_params); err != nil {
		panic(err)
	}

	device_info, err := LoadDeviceInfo(ps.ByName("device"))
	var val_Frequency, val_TransmissionPower, val_Modem, val_Antenna string

	// Fetch the control parameters before overwriting the file
	val_Frequency = device_info.Frequency
	val_TransmissionPower = device_info.TransmissionPower
	val_Modem = device_info.Modem
	val_Antenna = device_info.Antenna

	filename := device_info.DeviceType + ".control"

	f, err := os.Create(filename)
	check(err)
	defer f.Close()

	if val, ok := control_params["frequency"]; ok {
		val_Frequency = val
	}
	if val, ok := control_params["transmissionpower"]; ok {
		val_TransmissionPower = val
	}
	if val, ok := control_params["modem"]; ok {
		val_Modem = val
	}
	if val, ok := control_params["antenna"]; ok {
		val_Antenna = val
	}

	f.WriteString("[Frequency][" + val_Frequency + "]\n")
	f.WriteString("[TransmissionPower][" + val_TransmissionPower + "]\n")
	f.WriteString("[Modem][" + val_Modem + "]\n")
	f.WriteString("[Antenna][" + val_Antenna + "]\n")
	fmt.Fprint(w, "Device parameters updated !\n")
}

func Index(w http.ResponseWriter, r *http.Request, _ httprouter.Params) {
	fmt.Fprint(w, "Welcome!\n")
}

func main() {
	router := httprouter.New()
	router.GET("/", Index)
	router.GET("/view/:device", ViewHandler)
	router.GET("/edit/:device", EditHandler)
	router.GET("/save/:device", SaveHandler)
	router.GET("/device/:device/param/:param", GetParam)
	router.GET("/device/:device", GetAllDeviceParams)
	router.GET("/device/:device/status", GetAllDeviceStatus)
	router.POST("/device/:device/control", SetControlParam)
	log.Fatal(http.ListenAndServe(":8080", router))
}
