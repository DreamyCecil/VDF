# VDF

This is a simple reader/writer of [Valve Data Format (a.k.a. KeyValues)](https://developer.valvesoftware.com/wiki/KeyValues) written in ANSI C.

This library was made as a sort of an exercise in C, since I mainly use C++.

Hopefully no memory leaks! But if there still are, feel free to submit an issue or a pull request.  
I tried making sure that all allocated memory was fully and properly freed before program termination.

See [`DOCUMENTATION.md`](DOCUMENTATION.md) file for more information.

# Features

### Reading from character buffers & files
- Character buffers may be null-terminated or limited to a maximum size.
- The files are parsed using `fopen()` with `"rb"` and reading the contents into a character buffer.
- The ability to create versatile contexts for reading data in a specific way, as well as functions for quick one-line parsing.

### Writing into character buffers & files
- Character buffers are created and expanded by the specified step size on the fly, without having to do it manually.
- The files are written using `fopen()` with `"w"` in order to insert platform-specific line breaks.

### Other
- Keys and string values of any length.
- Support for CPP-styled single-line comments (`//`) and C-styled block comments (`/* */`).
- Context flags for toggling specific features:
  - Support for escape sequences in strings (**ON** by default).
  - Support for multiple values under the same key name (**ON** by default).
  - Value replacement in duplicate keys, if multi-key support is disabled (**ON** by default).
- Case-insensitive `#base` & `#include` macro support that includes files from absolute paths or relative to the specified base directory.
  - The behavior of each inclusion macro is identical to Source SDK 2013.
  - Context flags for multi-key support and value replacement in duplicate keys are ignored when merging pairs using `#base` due to its unique behavior.

### Currently not supported
- Conditional statements before the pairs, e.g. `[$WIN32] "key" "value"`.
- Non-ASCII encodings.

# How to use

1. Include `keyvalues.c` in your project or link the library in your CMake project using `CMakeLists.txt`.
2. Include `keyvalues.h` in your C/C++ code.

Sample code with usage examples can be found [here](samples).

# License

This library is in the public domain. That means you can do absolutely anything you want with it, although I appreciate attribution.

It's also licensed under the MIT license, in case public domain doesn't work for you. Every source file includes an explicit dual-license for you to choose from.
