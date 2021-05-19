#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
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

class device_params {
    public:
    std::string m_device_type;         // device type
    std::string m_device_family;       // device family
    std::set<std::string> status_params;  // device_status_params
    std::set<std::string> control_params;  // device_control_params
    void load(const std::string &filename);
};

void device_params :: load (const std::string &filename) {

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

int main (int argc, char * argv[]) {

    if (argc < 2) {
        std::cout << "Usage ./driver <filepath>";
        exit(0);
    }

    // Parse the isode-radio.xml driver configuration file and store the device status and
    // device control params.
    try {
        device_params dp;
        dp.load(std::string(argv[1]));
        std::cout << "\nSuccess\n";
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << "\n";
    }

    cbor item;

    while(1) {

        std::cout << "\nWaiting to receive data...." << std::endl;

        item.read (std::cin);
        std::string received_str = cbor :: debug (item);

        std::cout << "\nPrinting data...." << std::endl;
        std::cout << cbor::debug (item) << std::endl;
        //std::cout << "\nDecoded CBOR : ";
        std::ostringstream obj;
        //item.write(std::cout);
        item.write(obj);
        std::string got = obj.str();
        std::cout << "\nDecoded CBOR : " << got << std::endl;
    }
    return 0;
}