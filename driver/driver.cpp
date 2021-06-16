#include "driver.h"

namespace pt = boost::property_tree;
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;

using net::ip::tcp;
using boost::property_tree::ptree;
using boost::property_tree::write_json;
using boost::property_tree::read_json;

Driver :: Driver(std::string dev_name, std::string arg_schema_file, std::string arg_std_params_file) {

    device_name = dev_name;
    schema_file = arg_schema_file;
    std_params_file = arg_std_params_file;

    std::string param_types [] = {"Integer", "String", "Boolean",
                                    "DateTime", "Enumerated", "AlertType"};
    ptype.insert(param_types, param_types + sizeof(param_types)/sizeof(param_types[0]));

    status_msg_format = "<Status><Device>" + dev_name + "</Device><DeviceType>_devicetype_</DeviceType><Param>_paramname_</Param><_paramtype_>_paramvalue_</_paramtype_></Status>\n";
}

Driver :: ~Driver() {

}

std :: string Driver :: GetStatusMsgFormat(void) {
    return status_msg_format;
}

std :: string Driver :: GetDeviceName(void) {
    return device_name;
}

void Driver :: GetParamDetails (const std::string& rb_msg,
                                std::string& param_category,
                                std::string& param_name,
                                std::string& param_type,
                                std::string& param_value) {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    /*  Below is the example of a control message from RB server
        <Control>
            <Device>Radio</Device>
            <DeviceType>IsodeRadio</DeviceType>
            <Param>Frequency</Param>
            <Integer>22917</Integer>
        </Control>
    */

    // Replace "Status" with "Control" case sensitive in the below block.
    std :: regex param_regex("<.*<Param>(.*)</Param>(.*)</Control>");
    std :: smatch param_match;

    if ( std::regex_search(rb_msg, param_match, param_regex) ) {

        if ( param_name_type.find(param_match[1]) != param_name_type.end() ) {
            param_category = "CONTROL";
            param_name = param_match[1];
            param_type = param_name_type[param_name];

            std :: string encaps_value(param_match[2]);
            std :: regex value_regex("<.+>(.*)</.+>");
            std :: smatch value_match;

            if ( std::regex_search(encaps_value, value_match, value_regex) ) {
                param_value = value_match[1];
            }
        }  else if ( param_match[1] == "SendParameters" ) {
            param_category = "REFCONTROL";
            param_name = param_match[1];
            param_type = "NONE";
        }
    }
}

void Driver :: InitLogging (void) {

    logging::add_file_log
    (
        keywords::file_name = "/tmp/" + device_name + "_%N.log",        /*< file name pattern >*/
        keywords::rotation_size = 10 * 1024 * 1024,                                   /*< rotate files every 10 MiB... >*/
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0), /*< ...or at midnight >*/
        keywords::format = "[%TimeStamp%]: %Message%",                                /*< log record format >*/
        keywords::auto_flush = true
    );

    logging::core::get()->set_filter
    (
        logging::trivial::severity >= logging::trivial::info
    );

    logging::add_common_attributes();
}

// Parse the device schema XML and standard parameters XML and do the below
// 1. Extract the Device Status & Device Control Params
// 2. Extract the Reference Status & Reference Control Params
// 3. Extract the Standard Parameters

void Driver :: Load () {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    // Create empty property tree object
    pt::ptree device_tree;

    // Parse the XML into the property tree.
    pt::read_xml(schema_file, device_tree);

    device_type = device_tree.get<std::string>("AbstractDeviceSpecification.DeviceType");
    device_family = device_tree.get<std::string>("AbstractDeviceSpecification.DeviceFamily");

    status_msg_format = std::regex_replace(status_msg_format, std::regex("_devicetype_"), device_type);

    BOOST_LOG_SEV(lg, info) << "Device Type : [" << device_type << "] Device Family : [" \
        << device_family << "]";

    BOOST_LOG_SEV(lg, info) << "Fetching device status parameters from file [" << schema_file << "]";

    // Get list of device status params and their type from the xml schema file.
    for (auto& v : device_tree.get_child("AbstractDeviceSpecification.DeviceStatusParameters")) {
        bool found = false;
        std::string param_name("");
        for (auto& p : v.second) {
            std::string tag = p.first.data();
            // Store the param and the value. Since we can't fetch the device status value at this stage
            // set it to "".
            if (tag == "ParameterName") {
                param_name_val.insert(std::pair<std::string,std::string>(p.second.data(), ""));
                found = true;
                param_name = p.second.data();
            }
            // If the next found tag is param_type (String, Integer, Boolean,...),
            // store the earlier found param and param type.
            else if (found and ptype.find(tag) != ptype.end()) {
                param_name_type.insert(std::pair<std::string,std::string>(param_name, tag));
                BOOST_LOG_SEV(lg, info) << "Param [" << param_name << "], Type [" << tag << "]";
                found = false;
            }
        }
    }

    BOOST_LOG_SEV(lg, info) << "Fetching device control parameters from file [" << schema_file << "]";

    // Get list of device control params and their type from the xml schema file.
    for (auto& v : device_tree.get_child("AbstractDeviceSpecification.DeviceControlParameters")) {
        bool found = false;
        std::string param_name("");
        for (auto& p : v.second) {
            std::string tag = p.first.data();
            // Store the param and the value. Since we can't fetch the device status value at this stage
            // set it to "".
            if (tag == "ParameterName") {
                param_name_val.insert(std::pair<std::string,std::string>(p.second.data(), ""));
                found = true;
                param_name = p.second.data();
            }
            // If the next found tag is param_type (String, Integer, Boolean,...),
            // store the earlier found param and param type.
            else if (found and ptype.find(tag) != ptype.end()) {
                param_name_type.insert(std::pair<std::string,std::string>(param_name, tag));
                BOOST_LOG_SEV(lg, info) << "Param [" << param_name << "], Type [" << tag << "]";
                found = false;
            }
        }
    }

    // Get list of referenced status params from the xml schema file and store the value as ""
    // as no value could be fetched at this stage.
    for (auto& v : device_tree.get_child("AbstractDeviceSpecification.ReferencedStatusParameters")) {
         param_name_val.insert(std::pair<std::string, std::string>(v.second.data(),""));
    }

    pt::ptree stdparams_tree;
    pt::read_xml(std_params_file, stdparams_tree);

    // Get list of standard params and their types..
    BOOST_LOG_SEV(lg, info) << "Fetching standard parameters and their types from file [" << std_params_file << "]";

    for (auto& v : stdparams_tree.get_child("StandardParameterList")) {
        bool found = false;
        std::string param_name("");
        std::string param_type("");
        for (auto& p : v.second) {
            std::string tag = p.first.data();
            // Store the param
            if (tag == "ParameterName") {
                found = true;
                param_name = p.second.data();
            }
            // If the next found tag is param_type (String, Integer, Boolean,...),
            // store the earlier found param and param type.
            else if (found and ptype.find(tag) != ptype.end()) {
                param_name_type.insert(std::pair<std::string,std::string>(param_name, tag));
                BOOST_LOG_SEV(lg, info) << "Param [" << param_name << "], Type [" << tag << "]";
                found = false;
            }
        }
    }
}

// SendStatus function would send the status of all the device params (Status/Control/RefControl) to RB server
// if send_all_param is set to true. If it is set to false, it would only send the status
// of the updated device status params.
void Driver :: SendStatus ( std::map<std::string, std::string>& current_params,
                                  bool send_all_param ) {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    // If only updated params are to be sent,
    // keep only the updated entries and remove the rest.
    // Also update the current params.
    if ( send_all_param == false ) {
        for ( const auto& entry : param_name_val ) {
            std::map<std::string, std::string>::iterator it;
            it = current_params.find(entry.first);
            if ( it != current_params.end() ) {
                // This device status param value has not been updated.
                // Hence remove it.
                if ( current_params[entry.first] == entry.second ) {
                    current_params.erase(it);
                } else {
                    // Update the in memory status_param_val
                    param_name_val[entry.first] = current_params[entry.first];
                }
            }
        }
    }

    for ( const auto& entry : current_params ) {
        std::string msg = status_msg_format;
        msg = std::regex_replace(msg, std::regex("_paramname_"), entry.first);
        msg = std::regex_replace(msg, std::regex("_paramtype_"), param_name_type[entry.first]);
        msg = std::regex_replace(msg, std::regex("_paramvalue_"), entry.second);

        // Update the in memory device status params after all the device status params are sent.
        // When only the updated params are to be sent back the status_param_val map gets updated
        // in the above if block.
        if ( send_all_param )
            param_name_val[entry.first] = entry.second;

        BOOST_LOG_SEV(lg, info) << "Sending device status params to RB : [" << msg << "]";
        // Create a CBOR status message before sending it to STDOUT
        cbor status_msg(msg);
        cbor :: binary data = cbor :: encode(status_msg);
        cbor item = cbor :: tagged (24, data);
        item.write(std::cout);
        fflush(stdout);
    }
}

IsodeRadioDriver :: IsodeRadioDriver (std::string dev_host, std::string dev_port, std::string dev_name,
                                      std::string schema_file, std::string std_params_file)
                    : Driver(dev_name, schema_file, std_params_file) {
    device_host = dev_host;
    device_port = dev_port;

    MONITOR_TIME = 60;
    // HTTP version
    version = 11;
}

// Send the HTTP Get request to the web device and fetch all the
// device status and deivce control params.
std :: string IsodeRadioDriver :: HTTPGet (const std::string& target, std::map<std::string, std::string>& stat_param_value) {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    BOOST_LOG_SEV(lg, info) << "Sending HTTP Get request to device host : [" << \
        device_host << "], device port : [" << device_port << "], Device Target [" << target << "]\n";

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
        // std::cout << res << std::endl;

        // Gracefully close the socket
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};

        std :: stringstream response;
        response << res.body();

        ptree pt;
        read_json(response, pt);

        /*
        Status Message Format
        <Status>\
        <Device>_devicename_</Device>\
        <DeviceType>_devicetype_</DeviceType>\
        <Param>_paramname_</Param>\
        <_paramtype_>_paramvalue_</_paramtype_>\
        </Status>
        */

        // Example : ptree with one entry would have something like { "PowerSupplyConsumption" : "31" }
        // status_param_type is a map like { "PowerSupplyConsumption" : "Integer" }
        for (ptree::const_iterator it = pt.begin(); it != pt.end(); ++it) {

            std::string param = it->first;
            std::string value = it->second.get_value<std::string>();
            stat_param_value.insert(std::pair<std::string,std::string>(param, value) );
        }
        // If we get here then the connection is closed gracefully
    } catch ( std::exception const& e ) {
        BOOST_LOG_SEV(lg, info) << "Error : " << e.what() << std::endl;
        return "ERROR";
    }
    return "SUCCESS";
}

// Send the HTTP Post request to the web device and set the
// device control params.
std :: string IsodeRadioDriver :: HTTPPost (const std::string& target,
                                  const std::string& param,
                                  const std::string& value) {
    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    BOOST_LOG_SEV(lg, info) << "Sending HTTP Post request to device host : [" << \
        device_host << "], device port : [" << device_port << "], Device Target [" << target << "]\n";

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
        root.put (param, value);
        std::ostringstream buf;
        write_json (buf, root, false);
        std::string json = buf.str();

        BOOST_LOG_SEV(lg, info) << "Composed JSON message : " << json;

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
        // std::cout << res << std::endl;

        // Gracefully close the socket
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if( ec && ec != beast::errc::not_connected )
            throw beast::system_error{ec};

        // If we get here then the connection is closed gracefully
    } catch ( std::exception const& e ) {
        BOOST_LOG_SEV(lg, info) << "Error: " << e.what();
        return "ERROR";
    }
    return "SUCCESS";
}

// Send the HTTP request ( Get / Post ) to the web device based
// on the message received from the red black server.
void IsodeRadioDriver :: SendHTTPRequest (const std::string& rb_msg) {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    BOOST_LOG_SEV(lg, info) << "Received message from RB : [" << rb_msg << "]";

    std :: string param_category("");
    std :: string param_name("");
    std :: string param_type("");
    std :: string param_value("");

    Driver :: GetParamDetails (rb_msg, param_category, param_name, param_type, param_value);
    BOOST_LOG_SEV(lg, info) << "Param Category : [" << param_category << "]" << " Name : [" << param_name \
        << "] Value : [" << param_value << "]";

    std :: string target("/device/" + Driver :: GetDeviceName() + "/control");

    // Send HTTP Post Request
    if ( param_category == "CONTROL" ) {

        std:: string response = HTTPPost(target, param_name, param_value);
        if ( response == "SUCCESS" ) {
            std::string msg = Driver :: GetStatusMsgFormat();
            msg = std::regex_replace(msg, std::regex("_paramname_"), param_name);
            msg = std::regex_replace(msg, std::regex("_paramtype_"), param_type);
            msg = std::regex_replace(msg, std::regex("_paramvalue_"), param_value);
            msg = std::regex_replace(msg, std::regex("Control"), "Status");
            msg += "\n";
            BOOST_LOG_SEV(lg, info) << "Sending status messages : [" << msg << "]";
            // Create a CBOR referenced status message before sending it to STDOUT
            cbor status_msg(msg);
            cbor :: binary data = cbor :: encode(status_msg);
            cbor item = cbor :: tagged (24, data);
            item.write(std::cout);
            fflush(stdout);
        }
    } else if ( param_category == "REFCONTROL") {
        if (param_name == "SendParameters") {
            bool all_params_flag = true;
            ReportStatusToRB(all_params_flag);
        }
    }
}

void IsodeRadioDriver :: ReportStatusToRB (bool all_params_flag) {

    // Fetch the current params values
    std :: string device_name = Driver :: GetDeviceName();
    std :: string target("/device/" + device_name);
    std :: map<std::string, std::string> current_params;
    std :: string response = HTTPGet(target, current_params);

    if ( response == "SUCCESS" ) {
        // Send the values of the parameters to RB
        Driver :: SendStatus(current_params, all_params_flag);
    }
}

void IsodeRadioDriver :: SendMsgToDevice (const std::string& rb_msg) {
    SendHTTPRequest(rb_msg);
}

void IsodeRadioDriver :: Start (void) {

    Driver :: InitLogging();
    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    try {
        Driver :: Load();
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << "\n";
    }

    bool all_params_flag = true;
    // Report initial status of the device params to the RB server.
    ReportStatusToRB(all_params_flag);
    // Initial status of the device has been reported, now start the monitoring timer.
    auto start = std::chrono::system_clock::now();

    std::condition_variable cv;
    std::mutex mutex_;

    // thread to read msg from stdin and sending message to the device.
    std::thread io ([&] {
        while (true) {
            BOOST_LOG_SEV(lg, info) << "Waiting to receive data...";
            cbor item;
            item.read (std::cin);
            std::ostringstream obj;
            item.write(obj);
            std::string rb_msg = obj.str();
            std::lock_guard<std::mutex> lock(mutex_);
            SendMsgToDevice(rb_msg);
        }
    });

    // non-blocking thread to send the updated device status and referenced status parameters
    // to the RB
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(MONITOR_TIME));
        // MONITOR_TIME has expired so send a status update.
        // Get the Device Status Params and Referenced Status Params
        // from the device and send the same to RB.
        std::lock_guard<std::mutex> lock_main(mutex_);
        BOOST_LOG_SEV(lg, info) << "Monitoring device status";
        all_params_flag = false;
        ReportStatusToRB(all_params_flag);
    }
}

int main (int argc, char * argv[]) {

    boost::program_options::options_description desc
        ("\nMandatory arguments marked with '*'.\n"
           "Invocation : <drive_executable> --host <hostname> --port <port> <device_name> <schema_file> <std_params_file>\nAgruments");

    desc.add_options ()
    ("host", boost::program_options::value<std::string>()->required(),
                 "Hostname.")
    ("port",  boost::program_options::value<std::string>()->required(),
                 "Port");

    boost::program_options::variables_map vm;

    try {
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).run(), vm);
        boost::program_options::notify(vm);
    } catch (boost::program_options::error& e) {
        std::cout << "ERROR: " << e.what() << "\n";
        std::cout << desc << "\n";
        return 1;
    }

    if (argc < 8) {
        std::cout << "Error !! Check usage below.\n";
        std::cout << "Invocation : <drive_executable> --host <hostname> --port <port> <device_name> <schema_file> <std_params_file>";
        exit(0);
    }

    // Parse the isode-radio.xml device configuration file and store
    // the device status and device control params.
    std :: string host(vm["host"].as<std::string>());
    std :: string port(vm["port"].as<std::string>());
    std :: string name(argv[5]);
    std :: string schemafile(argv[6]);
    std :: string stdparamsfile(argv[7]);

    IsodeRadioDriver radiodriver(host, port, name, schemafile, stdparamsfile);

    radiodriver.Start();
    return 0;
}