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
