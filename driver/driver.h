#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <exception>
#include <string>
#include <regex>
#include <algorithm>
#include <cctype>

// Device schema parsing
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>

// Sending HTTP Get / Post to devices
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

// Constructing JSON object
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>

// Driver logging
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sinks/text_file_backend.hpp>

#include "cbor11.h"

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

    boost::asio::io_context ioc;          // io_context is required for all I/O
    int version;

    public:

    Driver(std::string device_host, std::string device_port, std::string device_name);

    ~Driver();

    // Parse the XML device schema and store the device status & control params.
    void Load(const std::string& file_device_schema);

    // Run the driver program.
    void Start(void);

    // Run the driver program.
    void InitLogging(void);

    // Send an HTTP Request ( Get / Post ) to the rb devices based on message received from 
    // the red-black server.
    void SendHTTPRequest(const std::string& rb_msg);

    // Send HTTP Get request to the rb device and get the status using device status parameters.
    std::string HTTPGet(const std::string& target_device);

    // Send HTTP Post request to the rb device to modify device control parameters.
    std::string HTTPPost(const std::string& target_device, const std::string& param, const std::string& value);
};