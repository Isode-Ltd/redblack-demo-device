package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"html/template"
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
	VSWR                   string `json:"vswr"`
	PowerSupplyVoltage     string `json:"powersupplyvoltage"`
	PowerSupplyConsumption string `json:"powersupplyconsumption"`
	Temperature            string `json:"temperature"`
	SignalLevel            string `json:"signallevel"`
	// Control parameters
	Frequency         string `json:"frequency"`
	TransmissionPower string `json:"transmissionpower"`
	Modem             string `json:"modem"`
	Antenna           string `json:"antenna"`
}

func check(e error) {
	if e != nil {
		panic(e)
	}
}

func (p *Device) save() error {
	filename := p.DeviceType + ".txt"

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
	return err
}

func LoadDeviceInfo(devicetype string) (*Device, error) {

	filename := devicetype + ".txt"
	file, err := os.Open(filename)

	if err != nil {
		file, err = os.Create(filename)
	}

	check(err)
	defer file.Close()

	// Start reading from the file with a reader.
	scanner := bufio.NewScanner(file)
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
	return &Device{DeviceType: devicetype, VSWR: val_VSWR, PowerSupplyVoltage: val_PowerSupplyVoltage,
		PowerSupplyConsumption: val_PowerSupplyConsumption, Temperature: val_Temperature,
		SignalLevel: val_SignalLevel}, nil
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

func GetDeviceStatus(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

	device_info, err := LoadDeviceInfo(ps.ByName("device"))

	if err == nil {
		json.NewEncoder(w).Encode(device_info)
	}
}

func GetParamValue(w http.ResponseWriter, r *http.Request, ps httprouter.Params) {

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

func Index(w http.ResponseWriter, r *http.Request, _ httprouter.Params) {
	fmt.Fprint(w, "Welcome!\n")
}

func main() {
	router := httprouter.New()
	router.GET("/", Index)
	router.GET("/view/:device", ViewHandler)
	router.GET("/edit/:device", EditHandler)
	router.GET("/save/:device", SaveHandler)
	router.GET("/device/:device/param/:param", GetParamValue)
	router.GET("/device/:device", GetDeviceStatus)
	log.Fatal(http.ListenAndServe(":8080", router))
}
