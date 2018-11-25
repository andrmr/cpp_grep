# cpp_grep
Grep-like implementation in C++ 17

## Requirements
### Input
- receive two command line parameters
- first parameter is a path to a file or folder
- second parameter is the string to look for (max length 128 characters)
- directories shall be traversed recursively

### Output
- print formatted results to stdout, including file name and offset where a match occurs

### Implementation
- strictly C++ STL

## Tested on:
- MSVC Community 2017 15.8.6 on Windows 10 64-bit
- Clang 7 on Ubuntu 18.04 64-bit
- GCC 8 on Ubuntu 18.04 64-bit

## Known limitations and issues:
- std::filesystem locale and UTF filepaths
