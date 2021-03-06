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

#ifdef _WIN32
	setmode(fileno(stdout), O_BINARY);
	setmode(fileno(stdin), O_BINARY);
#endif
}

Driver :: ~Driver() {

}

std :: string Driver :: GetStatusMsgFormat(void) {
    return status_msg_format;
}

std :: string Driver :: GetDeviceName(void) {
    return device_name;
}

std :: string Driver :: GetDeviceType(void) {
    return device_type;
}

void Driver :: SendHeartBeat(int MONITOR_TIME) {
    std::time_t time_now = std::time(nullptr);
    std::string msg = status_msg_format;
    msg = std::regex_replace(msg, std::regex("_paramname_"), "Heartbeat");
    msg = std::regex_replace(msg, std::regex("_paramtype_"), param_name_type["Heartbeat"]);
    msg = std::regex_replace(msg, std::regex("_paramvalue_"), std::to_string(time_now + MONITOR_TIME));
    SendCBOR(msg);
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

    // Create empty property tree object
    pt::ptree param_tree;

    try {
        std :: stringstream ss;
        ss << rb_msg;
        read_xml(ss, param_tree);
    } catch (pt::xml_parser_error &e) {
        BOOST_LOG_SEV(lg, info) << "Failed to parse control xml message from RB. " << e.what();
    } catch (...) {
        BOOST_LOG_SEV(lg, info) << "Failed to read RB control message.";
    }

    for (auto& v : param_tree.get_child("Control")) {

        std::string attr = v.first.data();

        if (attr == "Param") {
            param_name = v.second.data();
            if (param_name_type.find(param_name) != param_name_type.end()) {
                param_category = "CONTROL";
                param_type = param_name_type[param_name];
            } else if (param_name == "SendParameters" || param_name == "Reset" || param_name == "PowerOff") {
                param_category = "REFCONTROL";
                param_type = "EMPTY";
            }
        } else if (v.first.data() == param_type && param_category == "CONTROL") {
            param_value = v.second.data();
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
            else if (found && ptype.find(tag) != ptype.end()) {
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
            else if (found && ptype.find(tag) != ptype.end()) {
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
            else if (found && ptype.find(tag) != ptype.end()) {
                param_name_type.insert(std::pair<std::string,std::string>(param_name, tag));
                BOOST_LOG_SEV(lg, info) << "Param [" << param_name << "], Type [" << tag << "]";
                found = false;
            }
        }
    }
}

void Driver :: SendCBOR (const std::string & msg) {
    // Create a CBOR status message before sending it to STDOUT
    cbor status_msg(msg);
    cbor :: binary data = cbor :: encode(status_msg);
    cbor item = cbor :: tagged (24, data);
    item.write(std::cout);
    fflush(stdout);
}

void Driver :: UpdateDeviceParam(const std::string& param, const std::string& value) {
    param_name_val[param] = value;
}

std::string Driver :: GetParamValue(const std::string& param) {
    return param_name_val[param];
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
                // This device param value has not been updated.
                // Hence remove it.
                if ( current_params[entry.first] == entry.second ) {
                    current_params.erase(it);
                } else {
                    // Update the in memory param_name_val
                    param_name_val[entry.first] = current_params[entry.first];
                }
            }
        }
    }

    // Alert and AlertMessage are sent to RB in SendAlert function, hence skip these while sending status.
    // Some params like DeviceTypeHash are set only on RB hence skip sending it.
    std::string params_to_skip[]  = {"Alert", "AlertMessage", "DeviceTypeHash"};
    std::set<std::string> skip(params_to_skip, params_to_skip + sizeof(params_to_skip)/sizeof(params_to_skip[0]));

    for ( const auto& entry : current_params ) {

        if (skip.find(entry.first) != skip.end())
            continue;

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
        SendCBOR(msg);
    }
}

IsodeRadioDriver :: IsodeRadioDriver (std::string dev_host, std::string dev_port, std::string dev_name,
                                      std::string schema_file, std::string std_params_file)
                    : Driver(dev_name, schema_file, std_params_file) {
    device_host = dev_host;
    device_port = dev_port;

    MONITOR_TIME = 5;
    // HTTP version
    version = 11;
}

// Send the HTTP Get request to the web device and fetch all the
// device status and deivce control params.
bool IsodeRadioDriver :: HTTPGet (const std::string& target, std::map<std::string, std::string>& stat_param_value) {

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
        return false;
    }
    return true;
}

// Send the HTTP Post request to the web device and set the
// device control params.
bool IsodeRadioDriver :: HTTPPost (const std::string& target,
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
        return false;
    }
    return true;
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

    // Send HTTP Post Request
    if ( param_category == "CONTROL" ) {
        std :: string target("/device/" + Driver :: GetDeviceName() + "/control");
        if (HTTPPost(target, param_name, param_value)) {
            std::string msg = Driver :: GetStatusMsgFormat();
            msg = std::regex_replace(msg, std::regex("_paramname_"), param_name);
            msg = std::regex_replace(msg, std::regex("_paramtype_"), param_type);
            msg = std::regex_replace(msg, std::regex("_paramvalue_"), param_value);
            msg = std::regex_replace(msg, std::regex("Control"), "Status");
            msg += "\n";
            BOOST_LOG_SEV(lg, info) << "Sending status messages : [" << msg << "]";
            // Create a CBOR referenced status message before sending it to STDOUT
            SendCBOR(msg);
        }
    } else if ( param_category == "REFCONTROL") {
        if (param_name == "SendParameters") {
            bool all_params_flag = true;
            ReportStatusToRB(all_params_flag);
        } else if (param_name == "Reset") {
            std :: string target("/device/" + Driver :: GetDeviceName() + "/reset");
            std :: map<std::string, std::string> current_params;
            if (HTTPGet(target, current_params)) {
                BOOST_LOG_SEV(lg, info) << "Device " << GetDeviceName() << " Reset Succcessfully !!";
                // Send the values of the parameters to RB
                bool all_params_flag = true;
                Driver :: SendStatus(current_params, all_params_flag);
            }
        } else if (param_name == "PowerOff") {
            std :: string target("/device/" + Driver :: GetDeviceName() + "/poweroff");
            std :: map<std::string, std::string> current_params;
            if (HTTPGet(target, current_params)) {
                BOOST_LOG_SEV(lg, info) << "Device " << GetDeviceName() << " Powered Off Succcessfully !!";
                // Send the values of the parameters to RB
                bool all_params_flag = true;
                Driver :: SendStatus(current_params, all_params_flag);
            }
        }
    }
}

void IsodeRadioDriver :: ReportStatusToRB (bool all_params_flag) {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    // Fetch the current params values
    std :: string device_name = Driver :: GetDeviceName();
    std :: string target("/device/" + device_name);
    std :: map<std::string, std::string> current_params;

    std::string status_msg = "<Status><Device>_devicename_</Device><DeviceType>_devicetype_</DeviceType><Param>Status</Param><Enumerated>Operational</Enumerated></Status>";
    status_msg = std::regex_replace(status_msg, std::regex("_devicename_"), device_name);
    status_msg = std::regex_replace(status_msg, std::regex("_devicetype_"), GetDeviceType());

    if (HTTPGet(target, current_params)) {
        if (Driver::GetParamValue("Status") == "Not Operational") {
            status_msg = std::regex_replace(status_msg, std::regex("Operational"), "Not Operational");
        }
        // Send the values of the parameters to RB
        Driver :: SendStatus(current_params, all_params_flag);
        BOOST_LOG_SEV(lg, info) << "Device [" << device_name << "] operational.";
    } else {
        // Device not responding.
        status_msg = std::regex_replace(status_msg, std::regex("Operational"), "Not Operational");
        BOOST_LOG_SEV(lg, info) << "Device [" << device_name << "] not responding.";
        Driver :: UpdateDeviceParam("Status", "Not Operational");
    }

    BOOST_LOG_SEV(lg, info) << "Sending device status to RB : [" << status_msg << "]";
    SendCBOR(status_msg);
}

void IsodeRadioDriver :: SendMsgToDevice (const std::string& rb_msg) {
    SendHTTPRequest(rb_msg);
}

void IsodeRadioDriver :: SendAlert () {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    std :: string device_name = Driver :: GetDeviceName();
    std :: string target("/device/" + device_name);
    std :: map<std::string, std::string> current_params;

    if (!HTTPGet(target, current_params))
        return;

    std::string msg = "<Status><Device>_devicename_</Device><DeviceType>_devicetype_</DeviceType><Param>Alert</Param><_alerttype_></_alerttype_><AlertMessage>_alertmessage_</AlertMessage></Status>";
    msg = std::regex_replace(msg, std::regex("_devicename_"), device_name);
    msg = std::regex_replace(msg, std::regex("_devicetype_"), GetDeviceType());
    msg = std::regex_replace(msg, std::regex("_alerttype_"), current_params["Alert"]);
    msg = std::regex_replace(msg, std::regex("_alertmessage_"), current_params["AlertMessage"]);

    BOOST_LOG_SEV(lg, info) << "Sending alert message to RB : [" << msg << "]";
    SendCBOR(msg);
}

void IsodeRadioDriver :: Start (void) {

    InitLogging();
    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    try {
        Load();
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << "\n";
    }

    bool all_params_flag = true;
    // Report initial status of the device params to the RB server.
    ReportStatusToRB(all_params_flag);

    // Initial status of the device has been reported, now start the monitoring timer.
    auto timenow = std::chrono::system_clock::now();

    // Send the first heart beat message
    SendHeartBeat(MONITOR_TIME);

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
            std::size_t first = rb_msg.find("<Control");
            rb_msg = rb_msg.erase(0, first);
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

        // Send heartbeat to RB
        SendHeartBeat(MONITOR_TIME);

        // Send alert message to RB
        SendAlert();

        // Send status of the updated params to RB
        all_params_flag = false;
        BOOST_LOG_SEV(lg, info) << "Querying status of the device";
        ReportStatusToRB(all_params_flag);
    }
}

int main (int argc, char * argv[]) {

    boost::program_options::options_description desc
        ("\nMandatory arguments marked with '*'.\n"
           "Invocation : <driver_executable> --host <hostname> --port <port> --device_name <device_name> --schema_file <filename> --std_params_file <filename>\nArguments");

    desc.add_options ()
    ("host", boost::program_options::value<std::string>()->required(),
                 "* Hostname.")
    ("port",  boost::program_options::value<std::string>()->required(),
                 "* Port")
    ("device_name",  boost::program_options::value<std::string>()->required(),
                 "* Device Name")
    ("schema_file",  boost::program_options::value<std::string>()->required(),
                 "* Schema File")
    ("std_params_file",  boost::program_options::value<std::string>()->required(),
                 "* Standard Params File");

    boost::program_options::variables_map vm;

    try {
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).run(), vm);
        boost::program_options::notify(vm);
    } catch (boost::program_options::error& e) {
        std::cout << "ERROR: " << e.what() << "\n";
        std::cout << desc << "\n";
        return 1;
    }

    if (argc < 11) {
        std::cout << "Error !! Check usage below.\n";
        std::cout << "Invocation : <driver_executable> --host <hostname> --port <port> --device_name <device_name> --schema_file <filename> --std_params_file <filename>";
        exit(0);
    }

    // Parse the isode-radio.xml device configuration file and store
    // the device status and device control params.
    std :: string host(vm["host"].as<std::string>());
    std :: string port(vm["port"].as<std::string>());
    std :: string name(vm["device_name"].as<std::string>());
    std :: string schemafile(vm["schema_file"].as<std::string>());
    std :: string stdparamsfile(vm["std_params_file"].as<std::string>());

    IsodeRadioDriver radiodriver(host, port, name, schemafile, stdparamsfile);

    radiodriver.Start();
    return 0;
}