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
	Name string
	// Status parameters
	VSWR                   []byte
	PowerSupplyVoltage     []byte
	PowerSupplyConsumption []byte
	Temperature            []byte
	SignalLevel            []byte
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

	string_VSWR := string(p.VSWR)
	string_PowerSupplyVoltage := string(p.PowerSupplyVoltage)
	string_PowerSupplyConsumption := string(p.PowerSupplyConsumption)
	string_Temperature := string(p.Temperature)
	string_SignalLevel := string(p.SignalLevel)

	f.WriteString("[VSWR][" + string_VSWR + "]\n")
	f.WriteString("[PowerSupplyVoltage][" + string_PowerSupplyVoltage + "]\n")
	f.WriteString("[PowerSupplyConsumption][" + string_PowerSupplyConsumption + "]\n")
	f.WriteString("[Temperature][" + string_Temperature + "]\n")
	f.WriteString("[SignalLevel][" + string_SignalLevel + "]\n")
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
	var _VSWR, _PowerSupplyVoltage, _PowerSupplyConsumption []byte
	var _Temperature, _SignalLevel []byte

	for scanner.Scan() {
		line = scanner.Text()
		exp := regexp.MustCompile(`\[(.*?)\]\[(.*?)\]`)

		if line != "" {
			match := exp.FindStringSubmatch(line)
			if match[1] == "VSWR" {
				_VSWR = []byte(match[2])
			}
			if match[1] == "PowerSupplyVoltage" {
				_PowerSupplyVoltage = []byte(match[2])
			}
			if match[1] == "PowerSupplyConsumption" {
				_PowerSupplyConsumption = []byte(match[2])
			}
			if match[1] == "Temperature" {
				_Temperature = []byte(match[2])
			}
			if match[1] == "SignalLevel" {
				_SignalLevel = []byte(match[2])
			}
		}
	}
	return &DeviceStatus{Name: name, VSWR: _VSWR, PowerSupplyVoltage: _PowerSupplyVoltage,
		PowerSupplyConsumption: _PowerSupplyConsumption, Temperature: _Temperature,
		SignalLevel: _SignalLevel}, nil
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

	_VSWR := r.FormValue("VSWR")
	_PowerSupplyVoltage := r.FormValue("PowerSupplyVoltage")
	_PowerSupplyConsumption := r.FormValue("PowerSupplyConsumption")
	_Temperature := r.FormValue("Temperature")
	_SignalLevel := r.FormValue("SignalLevel")

	p := &DeviceStatus{Name: name,
		VSWR:                   []byte(_VSWR),
		PowerSupplyVoltage:     []byte(_PowerSupplyVoltage),
		PowerSupplyConsumption: []byte(_PowerSupplyConsumption),
		Temperature:            []byte(_Temperature),
		SignalLevel:            []byte(_SignalLevel)}

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
