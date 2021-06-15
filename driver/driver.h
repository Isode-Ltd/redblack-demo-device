#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <set>
#include <exception>
#include <string>
#include <regex>
#include <algorithm>
#include <cctype>
#include <map>
#include <chrono>
#include <ctime>
#include <condition_variable>
#include <mutex>
#include <thread>

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

// Command line arguments
#include <boost/program_options.hpp>

#include "cbor11.h"

class Driver {

    private:
    /* Store the device information */
    std::string device_type;       // Device type
    std::string device_family;     // Device family
    std::string device_name;       // Device name
    std::string schema_file;       // Device schema file
    std::string std_params_file;   // Device standard parameters file

    std::map<std::string, std::string> control_param_val;  // Map to store device control param and its value
    std::map<std::string, std::string> status_param_val;   // Map to store device status param and its value
    std::map<std::string, std::string> ref_param_val;      // Map to store referenced status param and its value

    std::map<std::string, std::string> control_param_type; // Map to store device control param and its type
    std::map<std::string, std::string> status_param_type;  // Map to store device status param and its type
    std::map<std::string, std::string> std_param_type;    // Map to store standard param name and its type

    std::set<std::string> ptype;              // Set to store the parameter type
    std::string status_msg_format;            // Generic format of the status message to be sent to RB server

    public:

    Driver(std::string dev_name, std::string schema_file, std::string std_params_file);

    ~Driver();

    std::string GetDeviceName(void);
    std::string GetStatusMsgFormat(void);

    // Initialize driver logging.
    void InitLogging(void);

    // This functions parses the message received from RB server and saves the param category, name, type
    // and value in the passed arguments.
    void GetParamDetails(const std::string& rb_msg, std::string& param_category,
                     std::string& param_name, std::string& param_type, std::string& param_value);

    // Parse the XML device schema and store the device status & control params.
    void Load();

    // Send the device status params to RB.
    void SendDeviceStatus(std::map<std::string, std::string>& current_dev_status, bool send_all_param);

    // Send the referenced status to RB.
    void SendRefStatus(std::map<std::string, std::string>& current_ref_params, bool send_all_param);

};

class IsodeRadioDriver : public Driver {

    private:
    std::string device_host;       // Device host
    std::string device_port;       // Device port

    boost::asio::io_context ioc;   // io_context is required for all I/O
    int version;                   // HTTP protocol version for sending GET / POST to web device.

    unsigned int MONITOR_TIME;     // Time in seconds to monitor the device status params and referenced status params

    public:

    IsodeRadioDriver(std::string device_host, std::string device_port, std::string device_name,
                     std::string schema_file, std::string std_params_file);

    // Run the driver program.
    void Start(void);

    // Send the message received from the RB to the device.
    void SendMsgToDevice(const std::string& rb_msg);

    // Report status of the device back to RB.
    void ReportStatusToRB(bool all_params_flag);

    // Send an HTTP Request ( Get / Post ) to the rb devices based on message received from 
    // the red-black server.
    void SendHTTPRequest(const std::string& rb_msg);

    // Send HTTP Get request to the rb device and get the status using device status parameters.
    std::string HTTPGet(const std::string& target_device, std::map<std::string, std::string>& status_param_val);

    // Send HTTP Post request to the rb device to modify device control parameters.
    std::string HTTPPost(const std::string& target_device, const std::string& param, const std::string& value);
};