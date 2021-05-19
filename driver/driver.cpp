#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <string>
#include <vector>
#include <set>
#include <exception>
#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include "cbor11.h"

namespace pt = boost::property_tree;
namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

class Driver {

    private:
    /* Store the device information */
    std::string device_host;              // device host
    std::string device_port;              // device port
    std::string m_device_type;            // device type
    std::string m_device_family;          // device family
    std::set<std::string> status_params;  // device_status_params
    std::set<std::string> control_params; // device_control_params

    net::io_context ioc;                  // io_context is required for all I/O

    public:
    //Driver(){}
    Driver(std::string host, std::string port):device_host(host),device_port(port)
    {}
    void SendHTTPRequest(const std::string &);
    void HTTPGet(const std::string &);
    void HTTPPost(const std::string &);
    void Load(const std::string &);
    void Start(void);
};


void Driver :: Load (const std::string &filename) {

    // Create empty property tree object
    pt::ptree tree;

    // Parse the XML into the property tree.
    pt::read_xml(filename, tree);

    m_device_type = tree.get<std::string>("AbstractDeviceSpecification.DeviceType");
    m_device_family = tree.get<std::string>("AbstractDeviceSpecification.DeviceFamily");

    std::cout << "Device Type..." << m_device_type << std::endl;
    std::cout << "Device Family..." << m_device_family << std::endl;

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

void Driver :: HTTPGet (const std::string& target) {

    try {
        // The io_context is required for all I/O
        net::io_context ioc;

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        // Look up the domain name
        auto const results = resolver.resolve(device_host, device_port);

        // Make the connection on the IP address we get from a lookup
        stream.connect(results);

        // Set up an HTTP GET request message
        int version = 11;
        http::request<http::string_body> req{http::verb::get, target, version};
        req.set(http::field::host, device_host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

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
    } catch(std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void Driver :: HTTPPost (const std::string& target) {

}

void Driver :: SendHTTPRequest (const std::string& got) {

    std::regex param_regex("<Param>(.*)</Param>");
    std::smatch param_match;

    if(std::regex_search(got, param_match, param_regex)) {
        if(status_params.find(param_match[1]) != status_params.end()) {
            std :: cout << "Device status param [" << param_match[1] << "] received. Will issue HTTP GET\n";

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
        std::cout << "Usage ./driver <filepath_device> <device_host> <device_port>";
        exit(0);
    }

    // Parse the isode-radio.xml device configuration file and store
    // the device status and device control params.
    std :: string host(argv[2]);
    std :: string port(argv[3]);

    Driver driver(host, port);
    try {
        driver.Load(std::string(argv[1]));
        std::cout << "\nSuccess\n";
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << "\n";
    }

    driver.Start();
    return 0;
}