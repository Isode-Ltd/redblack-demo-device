package main

import (
	"bufio"
	"html/template"
	"log"
	"net/http"
	"os"
	"regexp"
)

type Page struct {
	Title                  string
	VSWR                   []byte
	PowerSupplyVoltage     []byte
	PowerSupplyConsumption []byte
	Temperature            []byte
	SignalLevel            []byte
}

func FileExists(filename string) bool {
	info, err := os.Stat(filename)
	if os.IsNotExist(err) {
		return false
	}
	return !info.IsDir()
}

func check(e error) {
	if e != nil {
		panic(e)
	}
}

func (p *Page) save() error {
	filename := p.Title + ".txt"

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

func loadPage(title string) (*Page, error) {

	filename := title + ".txt"
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
	return &Page{Title: title, VSWR: _VSWR, PowerSupplyVoltage: _PowerSupplyVoltage,
		PowerSupplyConsumption: _PowerSupplyConsumption, Temperature: _Temperature,
		SignalLevel: _SignalLevel}, nil
}

func viewHandler(w http.ResponseWriter, r *http.Request, title string) {
	p, err := loadPage(title)
	if err != nil {
		http.Redirect(w, r, "/edit/"+title, http.StatusFound)
		return
	}
	renderTemplate(w, "view", p)
}

func editHandler(w http.ResponseWriter, r *http.Request, title string) {
	p, err := loadPage(title)
	if err != nil {
		p = &Page{Title: title}
	}
	renderTemplate(w, "edit", p)
}

func saveHandler(w http.ResponseWriter, r *http.Request, title string) {

	_VSWR := r.FormValue("VSWR")
	_PowerSupplyVoltage := r.FormValue("PowerSupplyVoltage")
	_PowerSupplyConsumption := r.FormValue("PowerSupplyConsumption")
	_Temperature := r.FormValue("Temperature")
	_SignalLevel := r.FormValue("SignalLevel")

	p := &Page{Title: title,
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
	http.Redirect(w, r, "/view/"+title, http.StatusFound)
}

var templates = template.Must(template.ParseFiles("edit.html", "view.html"))

func renderTemplate(w http.ResponseWriter, tmpl string, p *Page) {
	err := templates.ExecuteTemplate(w, tmpl+".html", p)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
	}
}

var validPath = regexp.MustCompile("^/(edit|save|view)/([a-zA-Z0-9]+)$")

func makeHandler(fn func(http.ResponseWriter, *http.Request, string)) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		m := validPath.FindStringSubmatch(r.URL.Path)
		if m == nil {
			http.NotFound(w, r)
			return
		}
		fn(w, r, m[2])
	}
}

func main() {
	http.HandleFunc("/view/", makeHandler(viewHandler))
	http.HandleFunc("/edit/", makeHandler(editHandler))
	http.HandleFunc("/save/", makeHandler(saveHandler))

	log.Fatal(http.ListenAndServe(":8080", nil))
}
