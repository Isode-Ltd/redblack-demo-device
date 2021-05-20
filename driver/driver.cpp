#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>

#include <string>
#include <vector>
#include <set>
#include <exception>
#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <algorithm>
#include <cctype>
#include "cbor11.h"

namespace pt = boost::property_tree;
namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>
using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;

class Driver {

    private:
    /* Store the device information */
    std::string device_host;              // device host
    std::string device_port;              // device port
    std::string device_name;              // device name
    std::string device_type;              // device type
    std::string device_family;            // device family
    std::set<std::string> status_params;  // device_status_params
    std::set<std::string> control_params; // device_control_params

    net::io_context ioc;                  // io_context is required for all I/O
    int version;

    public:

    Driver(std::string host, std::string port, std::string name):device_host(host),
    device_port(port),device_name(name) {
        version = 11;
    }
    void SendHTTPRequest(const std::string &);
    std :: string HTTPGet(const std::string &);
    std :: string HTTPPost(const std::string &);
    void Load(const std::string &);
    void Start(void);
};

void Driver :: Load (const std::string &filename) {

    // Create empty property tree object
    pt::ptree tree;

    // Parse the XML into the property tree.
    pt::read_xml(filename, tree);

    device_type = tree.get<std::string>("AbstractDeviceSpecification.DeviceType");
    device_family = tree.get<std::string>("AbstractDeviceSpecification.DeviceFamily");

    std::cout << "Device Type..." << device_type << std::endl;
    std::cout << "Device Family..." << device_family << std::endl;

    // Get list of device status params from the device xml file.
    std::cout << "\nDevice Status Parameters..." << std::endl;
    BOOST_FOREACH(pt::ptree::value_type &v,
        tree.get_child("AbstractDeviceSpecification.DeviceStatusParameters")) {

        BOOST_FOREACH(pt::ptree::value_type &p, v.second) {

            std::string param = p.first.data();
            if (param == "ParameterName") {
                std::cout << p.second.data() << std::endl;
                status_params.insert(p.second.data());
            }
        }
    }

    // Get list of device status params from the device xml file.
    std::cout << "\nDevice Control Parameters..." << std::endl;
    BOOST_FOREACH(pt::ptree::value_type &v,
        tree.get_child("AbstractDeviceSpecification.DeviceControlParameters")) {

        BOOST_FOREACH(pt::ptree::value_type &p, v.second) {
            std::string param = p.first.data();
            if (param == "ParameterName") {
                std::cout << p.second.data() << std::endl;
                control_params.insert(p.second.data());
            }
        }
    }
}

std :: string Driver :: HTTPGet (const std::string& target) {

    std :: cout << "HTTP Get request to => Device Host : [" << device_host << "], Device Port : [" \
        << device_port << "], Device Target [" << target << "]\n";

    try {
        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        // Look up the domain name
        auto const results = resolver.resolve(device_host, device_port);

        // Make the connection on the IP address we get from a lookup
        stream.connect(results);

        // Set up an HTTP GET request message
        http::request<http::string_body> req{http::verb::get, target, version};
        req.set(http::field::host, device_host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::string_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);

        // Write the message to standard out
        std::cout << res << std::endl;

        // Gracefully close the socket
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};

        std :: string response = res.body();
        response.erase(std::remove(response.begin(), response.end(), '\n'), response.end());
        std :: cout << "Response to HTTP Get [" << response << "]\n";
        return response;
        // If we get here then the connection is closed gracefully
    } catch(std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return "ERROR";
}

std :: string Driver :: HTTPPost (const std::string& target) {

    std :: cout << "HTTP Post request to => Device Host : [" << device_host << "], Device Port : [" \
        << device_port << "], Device Target [" << target << "]\n";

    try {

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        // Look up the domain name
        auto const results = resolver.resolve(device_host, device_port);

        // Make the connection on the IP address we get from a lookup
        stream.connect(results);

        // Set up an HTTP POST request message
        ptree root;
        root.put ("frequency", "100");
        root.put ("modem", "Audio");
        std::ostringstream buf;
        write_json (buf, root, false);
        std::string json = buf.str();

        std::cout << "JSON Message : " << json;

        http::request<http::string_body> req{http::verb::post, target, version};
        req.set(http::field::host, device_host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_type, "application/json");
        req.set(http::field::content_length, boost::lexical_cast<std::string>(json.size()));
        req.body() = json;
        req.prepare_payload();

        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);

        // Write the message to standard out
        std::cout << res << std::endl;

        // Gracefully close the socket
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};

        // If we get here then the connection is closed gracefully
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return "ERROR";
    }
    return EXIT_SUCCESS;
}

void Driver :: SendHTTPRequest (const std::string& got) {

    std::regex param_regex("<Param>(.*)</Param>");
    std::smatch param_match;

    if(std::regex_search(got, param_match, param_regex)) {
        if(status_params.find(param_match[1]) != status_params.end()) {
            std :: cout << "Device status param [" << param_match[1] << "] received. Will issue HTTP GET\n";
            std :: string param(param_match[1]);
            std :: transform(param.begin(), param.end(), param.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            std :: string target("/device/" + device_name + "/param/" + param);
            std :: string response = HTTPGet(target);
            if (response != "ERROR") {
                std :: cout << "================= Success ==================\n";
            }
        } else if(control_params.find(param_match[1]) != control_params.end()) {
            std :: cout << "Device control param [" << param_match[1] << "] received. Will issue HTTP POST\n";
        }
    }
}

void Driver :: Start(void) {

   cbor item;

    while(1) {

        std::cout << "\nWaiting to receive data...." << std::endl;

        item.read (std::cin);
        std::string received_str = cbor :: debug (item);

        std::cout << "\nPrinting data...." << std::endl;
        std::cout << cbor::debug (item) << std::endl;

        std::ostringstream obj;

        item.write(obj);
        std::string got = obj.str();
        std::cout << "\nDecoded CBOR : " << got << std::endl;

        SendHTTPRequest(got);
    }
}

int main (int argc, char * argv[]) {

    if (argc < 4) {
        std::cout << "Usage ./driver <device_schema> <device_host> <device_port> <device_name_optional>";
        exit(0);
    }

    // Parse the isode-radio.xml device configuration file and store
    // the device status and device control params.
    std :: string host(argv[2]);
    std :: string port(argv[3]);
    std :: string name("radio");
    if (argc > 4) {
        name = argv[4];
    }

    Driver driver(host, port, name);
    try {
        driver.Load(std::string(argv[1]));
        std::cout << "\nSuccess\n";
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << "\n";
    }

    driver.Start();
    return 0;
}