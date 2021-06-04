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

Driver :: Driver(std::string dev_host, std::string dev_port, std::string dev_name)
    :
    device_host(dev_host),
    device_port(dev_port),
    device_name(dev_name) {

        MONITOR_TIME = 30;
        // HTTP version
        version = 11;
        std::string param_types [] = {"Integer", "String", "Boolean",
                                      "DateTime", "Enumerated", "AlertType"};
        ptype.insert(param_types, param_types + sizeof(param_types)/sizeof(param_ptype[0]));

        status_msg_format = "<Status><Device>" + dev_name + "</Device><DeviceType>_devicetype_</DeviceType><Param>_paramname_</Param><_paramtype_>_paramvalue_</_paramtype_></Status>\n";
}

Driver :: ~Driver() {

}

void Driver :: InitLogging (void) {

    logging::add_file_log
    (
        keywords::file_name = "/tmp/" + device_name + "_%N.log",                      /*< file name pattern >*/
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

void Driver :: Load (const std::string &file_device_schema, const std::string &file_stdparams) {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    // Create empty property tree object
    pt::ptree device_tree;

    // Parse the XML into the property tree.
    pt::read_xml(file_device_schema, device_tree);

    device_type = device_tree.get<std::string>("AbstractDeviceSpecification.DeviceType");
    device_family = device_tree.get<std::string>("AbstractDeviceSpecification.DeviceFamily");

    status_msg_format = std::regex_replace(status_msg_format, std::regex("_devicetype_"), device_type);

    BOOST_LOG_SEV(lg, info) << "Device Type : [" << device_type << "] Device Family : [" \
        << device_family << "]";

    BOOST_LOG_SEV(lg, info) << "Fetching device status parameters";

    // Get list of device status params from the xml schema file.
    for (auto& v : device_tree.get_child("AbstractDeviceSpecification.DeviceStatusParameters")) {
        bool found = false;
        std::string param_name("");
        for (auto& p : v.second) {
            std::string tag = p.first.data();
            // Store the param and the value. Since we can't fetch the device status value at this stage
            // set it to "".
            if (tag == "ParameterName") {
                status_params_val.insert(std::pair<std::string,std::string>(p.second.data(), ""));
                found = true;
                param_name = p.second.data();
            }
            // If the next found tag is param_type (String, Integer, Boolean,...),
            // store the earlier found param and param type.
            else if (found and ptype.find(tag) != ptype.end()) {
                param_ptype.insert(std::pair<std::string,std::string>(param_name, tag));
                BOOST_LOG_SEV(lg, info) << "Param [" << param_name << "], Type [" << tag << "]";
                found = false;
            }
        }
    }

    BOOST_LOG_SEV(lg, info) << "Fetching device control parameters";

    // Get list of device status params from the xml schema file.
    for (auto& v : device_tree.get_child("AbstractDeviceSpecification.DeviceControlParameters")) {
        for (auto& p : v.second) {
            std::string param = p.first.data();
            if (param == "ParameterName") {
                BOOST_LOG_SEV(lg, info) << "[" << p.second.data() << "]";
                control_params.insert(p.second.data());
            }
        }
    }

    // Get list of referenced status params from the xml schema file and store the value as ""
    // as no value could be fetched at this stage.
    for (auto& v : device_tree.get_child("AbstractDeviceSpecification.ReferencedStatusParameters")) {
         ref_params_val.insert(std::pair<std::string, std::string>(v.second.data(),""));
    }

    pt::ptree stdparams_tree;
    pt::read_xml(file_stdparams, stdparams_tree);

    // Get list of standard params and their types..
    BOOST_LOG_SEV(lg, info) << "Fetching standard parameters and their types";

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
                stdparams_ptype.insert(std::pair<std::string,std::string>(param_name, tag));
                BOOST_LOG_SEV(lg, info) << "Param [" << param_name << "], Type [" << tag << "]";
                found = false;
            }
        }
    }
}

// Send the HTTP Get request to the web device and fetch all the
// device status and deivce control params.
std :: string Driver :: HTTPGet (const std::string& target, std::map<std::string, std::string>& stat_param_value) {

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
        // param_ptye is a map like { "PowerSupplyConsumption" : "Integer" }
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
std :: string Driver :: HTTPPost (const std::string& target,
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
void Driver :: SendHTTPRequest (const std::string& rb_msg) {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    BOOST_LOG_SEV(lg, info) << "Message from RB Server : [" << rb_msg << "]";
    /*  Below is the example of a control message from RB server
        <Control>
            <Device>Radio</Device>
            <DeviceType>IsodeRadio</DeviceType>
            <Param>Frequency</Param>
            <Integer>22917</Integer>
        </Control>
    */

    // Replace "Status" with "Control" case sensitive in the below block.
    std :: regex param_regex("<.*<Param>(.*)</Param>(.*)</Status>");
    std :: smatch param_match;

    if ( std::regex_search(rb_msg, param_match, param_regex) ) {

        if ( control_params.find(param_match[1]) != control_params.end() ) {

            std :: string msg(param_match[0]);
            std :: string param(param_match[1]);

            std :: string encaps_value(param_match[2]);
            std :: regex value_regex("<.+>(.*)</.+>");
            std :: smatch value_match;

            std :: string value("");
            if ( std::regex_search(encaps_value, value_match, value_regex) ) {
                value = value_match[1];
            }

            std :: transform(param.begin(), param.end(), param.begin(),
                           [](unsigned char c){ return std::tolower(c); });

            std :: string target("/device/" + device_name + "/control");

            // Send HTTP Post Request
            std :: string response = HTTPPost(target, param, value);

            if ( response == "SUCCESS" ) {
                msg = std::regex_replace(msg, std::regex("Status"), "Control");
                // Write to STDOUT in CBOR format.
                msg += "\n";
                cbor status_msg(msg);
                BOOST_LOG_SEV(lg, info) << "Sending status messages : [" << msg << "]";
                status_msg.write(std::cout);
            }
        } else if ( param_match[1] == "SendParameters" ) {
            // Fetch all parameters (Status & Control) using HTTP Get Request
            std :: string target("/device/" + device_name);
            std :: map<std::string, std::string> dev_status;
            std :: string response = HTTPGet(target, dev_status);
            if ( response == "SUCCESS" ) {
                for ( const auto& entry : dev_status ) {
                    std::string msg = status_msg_format;
                    msg = std::regex_replace(msg, std::regex("_paramname_"), entry.first);
                    msg = std::regex_replace(msg, std::regex("_paramtype_"), param_ptype[entry.first]);
                    msg = std::regex_replace(msg, std::regex("_paramvalue_"), entry.second);
                    msg += "\n";
                    cbor status_msg(msg);
                    BOOST_LOG_SEV(lg, info) << "Sending status and control params to RB : [" << msg << "]";

                    // Write to STDOUT in CBOR format.
                    status_msg.write(std::cout);
                }
            }
        }
    }
}

// SendDeviceStatus function would send the status of all the device status params to RB server
// if send_all_param is set to true. If it is set to false, it would only send the status
// of the updated device status params.
void Driver :: SendDeviceStatus (bool send_all_param) {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    std :: string target("/device/" + device_name + "/status");
    std :: map<std::string, std::string> current_dev_status;
    std :: string response = HTTPGet(target, current_dev_status);

    if ( response != "SUCCESS" )
        return;

    // If only updated device status params are to be sent,
    // keep only the updated entries and remove the rest.
    // Also update the current device status.
    if ( send_all_param == false ) {
        for ( const auto& entry : status_params_val ) {
            std::map<std::string, std::string>::iterator it;
            it = current_dev_status.find(entry.first);
            if ( it != current_dev_status.end() ) {
                // This device status param value has not been updated.
                // Hence remove it.
                if ( current_dev_status[entry.first] == entry.second ) {
                    current_dev_status.erase(it);
                } else {
                    status_params_val[entry.first] = current_dev_status[entry.first];
                }
            }
        }
    }

    for ( const auto& entry : current_dev_status ) {
        std::string msg = status_msg_format;
        msg = std::regex_replace(msg, std::regex("_paramname_"), entry.first);
        msg = std::regex_replace(msg, std::regex("_paramtype_"), param_ptype[entry.first]);
        msg = std::regex_replace(msg, std::regex("_paramvalue_"), entry.second);

        // Update the in memory device status params after all the device status params are sent.
        if ( send_all_param )
            status_params_val[entry.first] = entry.second;

        // Create a CBOR status message before sending it to STDOUT
        cbor status_msg(msg);
        BOOST_LOG_SEV(lg, info) << "Sending device status params to RB : [" << msg << "]";

        // Write to STDOUT in CBOR format.
        status_msg.write(std::cout);
    }
}

// SendRefStatus function would send the reference status params of the device to the RB server
void Driver :: SendRefStatus () {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    std :: string target("/device/" + device_name + "/ref");
    std :: map<std::string, std::string> current_ref_params;
    std :: string response = HTTPGet(target, current_ref_params);

    if ( response != "SUCCESS" )
        return;

    // If only updated referenced params are to be sent,
    // keep just the updated entries and remove the rest.
    // Also update the current referenced status.

    for ( const auto& entry : ref_params_val ) {
        std::map<std::string, std::string>::iterator it;
        it = current_ref_params.find(entry.first);
        if ( it != current_ref_params.end() ) {
            // This referenced status param value has not been updated,
            // hence remove it.
            if ( current_ref_params[entry.first] == entry.second ) {
                current_ref_params.erase(it);
            } else {
                ref_params_val[entry.first] = current_ref_params[entry.first];
            }
        }
    }

    for ( const auto& entry : current_ref_params ) {
        std::string msg = status_msg_format;
        msg = std::regex_replace(msg, std::regex("_paramname_"), entry.first);
        msg = std::regex_replace(msg, std::regex("_paramtype_"), stdparams_ptype[entry.first]);
        msg = std::regex_replace(msg, std::regex("_paramvalue_"), entry.second);

        cbor status_msg(msg);
        BOOST_LOG_SEV(lg, info) << "Sending referenced status params to RB : [" << msg << "]";

        // Write to STDOUT in CBOR format.
        status_msg.write(std::cout);
    }
}

void Driver :: Start (void) {

    cbor item;
    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    // Send the status of all the device parameters to RB
    bool all_params = true;
    SendDeviceStatus(all_params);

    auto start = std::chrono::system_clock::now();

    while(1) {

        BOOST_LOG_SEV(lg, info) << "Waiting to receive data...";

        std::ostringstream obj;
        item.read (std::cin);

        item.write(obj);
        std::string rb_msg = obj.str();
        SendHTTPRequest(rb_msg);

        all_params = false;

        auto end = std::chrono::system_clock::now();

        std::chrono::duration<double> elapsed_seconds = end - start;

        // Elapsed time it greater than monitor time. Get the Device Status Params and
        // Referenced Status Params from the device and send the same to RB.

        if ( int(elapsed_seconds.count()) > MONITOR_TIME ) {
            BOOST_LOG_SEV(lg, info) << "Time elapsed since last monitor is [" << int(elapsed_seconds.count()) << "] seconds";
            // Send the status of the updated device parameters to RB
            SendDeviceStatus(false);

            // Send the status of the updated referenced parameters to RB
            SendRefStatus();
            start = std::chrono::system_clock::now();
        }
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

    Driver driver(host, port, name);
    driver.InitLogging();

    try {
        driver.Load(std::string(argv[6]), std::string(argv[7]));
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << "\n";
    }

    driver.Start();
    return 0;
}