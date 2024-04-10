# 2048-probability-model
This is my attempt at using a probability model to mind optimal moves in 2048.
Check out my chicken scratch notes for more details.

# External Code

All code in the src/external directory is, as the name suggests, external and as such is governed by its own license. Because these are submodules, clone with `git clone git@github.com:orsonbraines/2048-probability-model.git --recursive`. If you have already cloned, you can load the submodules with `git submodule update --init --recursive`.

# Building
## Prerequisites
### System/Platform

I have tested builds using GCC on Ubuntu Linux and using MSVC on Windows, both with an x64 processor. If you would like to have support for other platforms, let me know.

### Python 3

Used to auto-generated code.

### Curses

Used for the Text User Interface (TUI).

### SQLite
Used for generating and storing tablebases.

## Ubuntu Step-by-Step

1. Python3, ncurses, and sqlite3 are probably installed already, but you can double check with `sudo apt install python3 libncurses-dev libsqlite3-dev`
2. Install g++, make, and cmake: `sudo apt install g++ make cmake`
3. Make a build directory: `mkdir build`
4. Configure CMake: `cd build && cmake ../src`
5. Run make: `make`

## Windows Step-by-Step

1. Install the latest version of Visual Studio (Community Edition is fine) from https://visualstudio.microsoft.com/downloads/
2. Install the source code for PDCurses from https://github.com/wmcbrine/PDCurses/releases
3. Follow the instructions in PDCurses-3.9/wincon/README.md to build pdcurses.lib. To build a 64-bit library, make sure to run in the 64-bit developer command prompt.
4. Download the latest sqlite3 amalgamation from https://www.sqlite.org/2024/sqlite-amalgamation-3450200.zip and copy sqlite3.c and sqlite3.h to this repo's src/external directory
5. Open the 2048-probability-model/src directory in Visual Studio as a CMake project
6. Make sure to match the configuration to the environment that you build PDCurses for (x64 vs x86). In the CMake configuration, define `CURSES_INCLUDE_PATH` and `CURSES_LIBRARY` so that CMake knows where to find PDCurses.
```
{
    "name": "x64-Debug",
    "generator": "Ninja",
    "configurationType": "Debug",
    "inheritEnvironments": [ "msvc_x64_x64" ],
    "buildRoot": "${projectDir}\\out\\build\\${name}",
    "installRoot": "${projectDir}\\out\\install\\${name}",
    "cmakeCommandArgs": "",
    "buildCommandArgs": "",
    "ctestCommandArgs": "",
    "variables": [
    {
        "name": "CURSES_INCLUDE_PATH",
        "value": "C:/path/to/PDCurses-3.9",
        "type": "PATH"
    },
    {
        "name": "CURSES_LIBRARY",
        "value": "C:/path/to/PDCurses-3.9/wincon/pdcurses.lib",
        "type": "PATH"
    }
    ]
}
```
6. Build All
