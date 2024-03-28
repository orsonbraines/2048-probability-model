# 2048-probability-model
This is my attempt at using a probability model to mind optimal moves in 2048.
Check out my chicken scratch notes for more details.

# Building
## Prerequisites
### System/Platform

I have tested builds using GCC on Ubuntu Linux and using MSVC on Windows, both with an x64 processor. If you would like to have support for other platforms, let me know.

### Python 3

Used to auto-generated code.

### Curses

Used for the Text User Interface (TUI).

## Ubuntu Step-by-Step

1. Python3 and ncurses are probably installed already, but you can double check with `sudo apt install python3 libncurses-dev`
2. Install g++, make, and cmake: `sudo apt install g++ make cmake`
3. Make a build directory: `mkdir build`
4. Configure CMake: `cd build && cmake ../src`
5. Run make: `make`
