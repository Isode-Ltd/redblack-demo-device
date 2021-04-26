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
)

type DeviceStatus struct {
	// Device name
	Name string `json:"name"`
	// Status parameters
	VSWR                   string `json:"vswr"`
	PowerSupplyVoltage     string `json:"powersupplyvoltage"`
	PowerSupplyConsumption string `json:"powersupplyconsumption"`
	Temperature            string `json:"temperature"`
	SignalLevel            string `json:"signallevel"`
}

func check(e error) {
	if e != nil {
		panic(e)
	}
}

func (p *DeviceStatus) save() error {
	filename := p.Name + ".txt"

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

func loadDeviceStatus(name string) (*DeviceStatus, error) {

	filename := name + ".txt"
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
	return &DeviceStatus{Name: name, VSWR: val_VSWR, PowerSupplyVoltage: val_PowerSupplyVoltage,
		PowerSupplyConsumption: val_PowerSupplyConsumption, Temperature: val_Temperature,
		SignalLevel: val_SignalLevel}, nil
}

func ViewHandler(w http.ResponseWriter, r *http.Request, name string) {
	p, err := loadDeviceStatus(name)
	if err != nil {
		http.Redirect(w, r, "/edit/"+name, http.StatusFound)
		return
	}
	RenderTemplate(w, "view", p)
}

func EditHandler(w http.ResponseWriter, r *http.Request, name string) {
	p, err := loadDeviceStatus(name)
	if err != nil {
		p = &DeviceStatus{Name: name}
	}
	RenderTemplate(w, "edit", p)
}

func SaveHandler(w http.ResponseWriter, r *http.Request, name string) {

	val_VSWR := r.FormValue("VSWR")
	val_PowerSupplyVoltage := r.FormValue("PowerSupplyVoltage")
	val_PowerSupplyConsumption := r.FormValue("PowerSupplyConsumption")
	val_Temperature := r.FormValue("Temperature")
	val_SignalLevel := r.FormValue("SignalLevel")

	p := &DeviceStatus{Name: name,
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
	http.Redirect(w, r, "/view/"+name, http.StatusFound)
}

var templates = template.Must(template.ParseFiles("edit.html", "view.html"))

func RenderTemplate(w http.ResponseWriter, tmpl string, p *DeviceStatus) {
	err := templates.ExecuteTemplate(w, tmpl+".html", p)
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

func ReturnStatus(w http.ResponseWriter, r *http.Request) {
	device_status, err := loadDeviceStatus("radio")
	fmt.Println(device_status)
	if err == nil {
		status_data, err := json.Marshal(device_status)
		check(err)
		fmt.Println(string(status_data))
	}
}

func main() {

	http.HandleFunc("/view/", MakeHandler(ViewHandler))
	http.HandleFunc("/edit/", MakeHandler(EditHandler))
	http.HandleFunc("/save/", MakeHandler(SaveHandler))
	http.HandleFunc("/status", ReturnStatus)
	log.Fatal(http.ListenAndServe(":8080", nil))
}
