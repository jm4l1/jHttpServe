# CPPND: Capstone HTTP Server

This is the Capstone project for in the [Udacity C++ Nanodegree Program](https://www.udacity.com/course/c-plus-plus-nanodegree--nd213). This is a basic implementation of an Hypertext Transer Protocol (HTTP) based on the following RFC.
* [RFC 7230](https://tools.ietf.org/html/rfc7230)

## Dependencies for Running Locally
* cmake >= 3.7
  * All OSes: [click here for installation instructions](https://cmake.org/install/)
* make >= 4.1 (Linux, Mac), 3.81 (Windows)
  * Linux: make is installed by default on most Linux distros
  * Mac: [install Xcode command line tools to get make](https://developer.apple.com/xcode/features/)
  * Windows: [Click here for installation instructions](http://gnuwin32.sourceforge.net/packages/make.htm)
* gcc/g++ >= 5.4
  * Linux: gcc / g++ is installed by default on most Linux distros
  * Mac: same deal as make - [install Xcode command line tools](https://developer.apple.com/xcode/features/)
  * Windows: recommend using [MinGW](http://www.mingw.org/)
* [jm4l1/jjson](https://github.com/jm4l1/jjson)
## Basic Build Instructions

1. Clone this repo.
2. Add git submodules `git submodule add git@github.com:jm4l1/jjson.git lib/jjson`
3. Make a build directory in the top level directory: `mkdir build && cd build`
4. Compile: `cmake .. && make`
5. Run it: `./capstoneHttp`.