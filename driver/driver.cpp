#include "cbor11.h"
#include <iostream>
#include <fstream>
#include <string>
#include <regex>

using namespace std;

/*
string Hex_To_ASCII(string hex_str) {

    string ascii_str = "";
    for (size_t i = 0; i < hex_str.length(); i += 2) {
        // extract two characters from hex string
        string part = hex_str.substr(i, 2);

        // Convert to base 16 and typecast as the char
        char ch = stoul(part, nullptr, 16);
        ascii_str += ch;
    }
    return ascii_str;
} */

int main() {

    /* cbor item = cbor::array {
    	12,
    	-12,
    	u8"イチゴ",
    	cbor::binary {0xff, 0xff},
    	cbor::array {"Alice", "Bob", "Eve"},
    	cbor::map {
    		{"CH", "Switzerland"},
    		{"DK", "Denmark"},
    		{"JP", "Japan"}
    	},
    	cbor::simple (0),
    	false,
    	nullptr,
    	cbor::undefined,
    	1.2
    }; 
    
    item.write (std::cout);
    // Convert to diagnostic notation for easy debugging
    std::cout << cbor::debug (item) << std::endl;
    
    // Encode
    cbor::binary data = cbor::encode (item);
    std::cout << cbor::debug (data) << std::endl;
    
    // Decode (if invalid data is given cbor::undefined is returned)
    // item = cbor::decode (data);
    // std::cout << cbor::debug (item) << std::endl; */
    
    cbor item;
    // regex regex_hex_string("h'(.*)'");

    while(1) {

        std::cout << "\nWaiting to receive data...." << std::endl;

        item.read (std::cin);
        string received_str = cbor :: debug (item);

        std::cout << "\nPrinting data...." << std::endl;
        std::cout << cbor::debug (item) << std::endl;
        std::cout << "\nDecoded CBOR : "; 
        item.write(std::cout);
        /*
        smatch hex_match;
        if (regex_search(received_str, hex_match, regex_hex_string)) {
            std::cout << "Extract : " << hex_match[1] << std::endl;
            std::cout << "Decoded CBOR 1: " << Hex_To_ASCII(hex_match[1]) << endl;
            std::cout << "Decoded CBOR 2: "; 
            item.write(std::cout);
        } */
    }
    
    // Write to any instance of std::ostream
    // item.write (std::cout);

    return 0;
}
