# CBOR11

A simple CBOR implementation written in C++. It decodes any valid CBOR, but
will always encode in the shortest definite form. Integers and floating-point
numbers are kept as distinct types when encoding. Tags are parsed and exposed
to the application. They are not interpreted in any way however.

## Types

* Unsigned integer (`unsigned long long`, `unsigned long`, `unsigned int`,
  `unsigned short`, `long long`, `long`, `int` and `short`)
* Negative integer (`long long`, `long`, `int` and `short`)
* Byte string (`cbor::binary`/`std::vector<unsigned char>`)
* Text string (`cbor::string`/`std::string`)
* Array of items (`cbor::array`/`std::vector<cbor>`)
* Map of pairs of items (`cbor::map`/`std::map<cbor,cbor>`)
* Tagged values (`cbor::tagged (tag, value)`)
* Simple values (`cbor::simple`, `bool` and `nullptr_t`)
* Floating-point numbers (`double` and `float`)

## Usage

```c++
#include "cbor11.h"

// Create complicated CBOR with simple code thanks to C++11
cbor item = cbor::array {
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

// Convert to diagnostic notation for easy debugging
std::cout << cbor::debug (item) << std::endl;

// Encode
cbor::binary data = cbor::encode (item);

// Decode (if invalid data is given cbor::undefined is returned)
item = cbor::decode (data);

// Read from any instance of std::istream
item.read (std::cin);

// Write to any instance of std::ostream
item.write (std::cout);
```

## Compilation

To enable all features you must compile with `-std=c++11`.

## Unlicense

Created 2014 Jakob Varmose Bentzen.

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
