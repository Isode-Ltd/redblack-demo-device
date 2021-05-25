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
        version = 11;
        std::string param_types [] = {"Integer", "String", "Boolean", "DateTime", "Enumerated"};
        ptype.insert(param_types, param_types + sizeof(param_types)/sizeof(param_ptype[0]));

        status_msg_format = "<Status><Device>" + dev_name + "</Device><DeviceType>_devicetype_</DeviceType><Param>_paramname_</Param><_paramtype_>_paramvalue_</_paramtype_></Status>";
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

// Parse the device schema XML and extract the Device Status & Device Control Params
void Driver :: Load (const std::string &file_device_schema) {

    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    // Create empty property tree object
    pt::ptree tree;

    // Parse the XML into the property tree.
    pt::read_xml(file_device_schema, tree);

    device_type = tree.get<std::string>("AbstractDeviceSpecification.DeviceType");
    device_family = tree.get<std::string>("AbstractDeviceSpecification.DeviceFamily");

    status_msg_format = std::regex_replace(status_msg_format, std::regex("_devicetype_"), device_type);

    BOOST_LOG_SEV(lg, info) << "Device Type : [" << device_type << "] Device Family : [" \
        << device_family << "]";

    BOOST_LOG_SEV(lg, info) << "Fetching device status parameters";

    // Get list of device status params from the xml schema file.
    BOOST_FOREACH(pt::ptree::value_type &v,
        tree.get_child("AbstractDeviceSpecification.DeviceStatusParameters")) {

        bool found = false;
        std::string tmp("");
        BOOST_FOREACH(pt::ptree::value_type &p, v.second) {

            std::string param = p.first.data();
            // Store the param
            if (param == "ParameterName") {
                status_params.insert(p.second.data());
                found = true;
                tmp = p.second.data();
            }
            // Store the param and param_type
            if (found and ptype.find(param) != ptype.end()) {
                param_ptype.insert(std::pair<std::string,std::string>(tmp, param));
                BOOST_LOG_SEV(lg, info) << "Param [" << tmp << "], Type [" << param << "]";
                found = false;
            }
        }
    }

    BOOST_LOG_SEV(lg, info) << "Fetching device control parameters";

    // Get list of device status params from the xml schema file.
    BOOST_FOREACH(pt::ptree::value_type &v,
        tree.get_child("AbstractDeviceSpecification.DeviceControlParameters")) {

        BOOST_FOREACH(pt::ptree::value_type &p, v.second) {
            std::string param = p.first.data();
            if (param == "ParameterName") {
                BOOST_LOG_SEV(lg, info) << "[" << p.second.data() << "]";
                control_params.insert(p.second.data());
            }
        }
    }
}

// Send the HTTP Get request to the web device and fetch all the
// device status and deivce control params.
std :: string Driver :: HTTPGet (const std::string& target) {

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
        std::cout << res << std::endl;

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
            std::string msg = status_msg_format;
            msg = std::regex_replace(msg, std::regex("_paramname_"), it->first);
            msg = std::regex_replace(msg, std::regex("_paramtype_"), param_ptype[it->first]);
            std::string value = it->second.get_value<std::string>();
            msg = std::regex_replace(msg, std::regex("_paramvalue_"), value);

            cbor status_msg(msg);
            BOOST_LOG_SEV(lg, info) << "Sending status messages : [" << msg << "]";
            //std::cout << "\n====================================================\n";
            // Write to STDOUT in CBOR format.
            status_msg.write(std::cout);
            //std::cout << "\n====================================================\n";
        }
        // If we get here then the connection is closed gracefully
    } catch(std::exception const& e) {
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
    } catch (std::exception const& e) {
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

    if (std::regex_search(rb_msg, param_match, param_regex)) {

        if (control_params.find(param_match[1]) != control_params.end()) {

            std :: string msg(param_match[0]);
            std :: string param(param_match[1]);

            std :: string encaps_value(param_match[2]);
            std :: regex value_regex("<.+>(.*)</.+>");
            std :: smatch value_match;

            std :: string value("");
            if (std::regex_search(encaps_value, value_match, value_regex)) {
                value = value_match[1];
            }

            std :: transform(param.begin(), param.end(), param.begin(),
                           [](unsigned char c){ return std::tolower(c); });

            std :: string target("/device/" + device_name + "/control");

            // Send HTTP Post Request
            std :: string response = HTTPPost(target, param, value);

            if (response == "SUCCESS") {
                msg = std::regex_replace(msg, std::regex("Status"), "Control");
                // Write to STDOUT in CBOR format.
                cbor status_msg(msg);
                BOOST_LOG_SEV(lg, info) << "Sending status messages : [" << msg << "]";
                status_msg.write(std::cout);
            }
        } else if ( param_match[1] == "SendParameters" ) {
            std :: string target("/device/" + device_name);
            std :: string response = HTTPGet(target);
            if (response == "SUCCESS") {
                BOOST_LOG_SEV(lg, info) << "Status and control params successfully sent to RB server.";
            }
        }
    }
}

void Driver :: Start (void) {

    cbor item;
    using namespace logging::trivial;
    src::severity_logger<severity_level> lg;

    while(1) {

        BOOST_LOG_SEV(lg, info) << "Waiting to receive data...";

        item.read (std::cin);
        std::string received_str = cbor :: debug (item);

        std::ostringstream obj;

        item.write(obj);
        std::string rb_msg = obj.str();

        SendHTTPRequest(rb_msg);
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
    driver.InitLogging();

    try {
        driver.Load(std::string(argv[1]));
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << "\n";
    }
    driver.Start();
    return 0;
}