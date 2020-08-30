## jHttpServe : A Basic HTTP/1.1 Server

This is a basic implementation of an Hypertext Transfer Protocol (HTTP) based on the following RFCs. It implements the basic functionality of HTTP/1.1, allowing for retrieval and uploading of files.
* [RFC 7230](https://tools.ietf.org/html/rfc7230) - Hypertext Transfer Protocol (HTTP/1.1): Message Syntax and Routing
* [RFC 7231](https://tools.ietf.org/html/rfc7231) - Hypertext Transfer Protocol (HTTP/1.1): Semantics and Content
* [RFC 7578](https://tools.ietf.org/html/rfc7578) - Returning Values from Forms: multipart/form-data

## Supported Operated Systems
jHttpServe currently support building on Linux and MacOs.

## Dependencies for Running Locally
* cmake >= 3.7
  * All OSes: [click here for installation instructions](https://cmake.org/install/)
* make >= 4.1 (Linux, Mac)
  * Linux: make is installed by default on most Linux distros
  * Mac: [install Xcode command line tools to get make](https://developer.apple.com/xcode/features/)packages/make.htm)
* Compiler supporting implementation of `std::filesystem`
  * gcc/g++ >= 9.3
    * Linux: gcc / g++ is installed by default on most Linux distros
    * Mac: same deal as make - [install Xcode command line tools](https://developer.apple.com/xcode/features/)
  * clang >= 9.3
* Json parsing library [jm4l1/jjson](https://github.com/jm4l1/jjson)
## Basic Build Instructions

1. Clone this repo.
2. Init git submodules `git submodule init`
3. Checkout master branch of submoduke `cd lib && git checkout master`
4. Make a build directory in the top level directory: `mkdir build && cd build`
5. Compile: `cmake .. && make`
6. Run it: `./jHttpServe`.

## Using the application
Run application with `-h` flag to get help menu
```bash
$ cd build
$ ./jHttpServe -h
Usage: ./jHttpServe [-f config_file] [-h] [-p port] [-t timeout]
	-f config_file : location of configuration file in .json format
	-p port : Tcp port to accept server connections
	-t timeoutt : Time out of server to close connection
	-h : show help menu
```

Run application with `-f` specifying a JSON configuration file. The configuration file format is show below.
``` json
{
    "port" : 12345,
    "timeout" : 100,
    "server_name" :"Http Server / 1.0",
    "allowed_methods" : ["GET","OPTIONS","POST","DELETE"],
    "web_dir" : "/Users/mali/Developer/mali/cppfiles/CppND-Capstone-http-server/www"
}
```
A config file must be specified and exists. The file must contain the `port` & `web_dir` values or the application throws an error an exists.
```bash
$./jHttpServe -f ../server.json
config file loaded
[CreateSocket] - Starting SocketServer
[CreateSocket] - Socket Created Successfully with FD:  3
[CreateSocket] - Socket bound to port 12345
[SocketServer] - Listening on * 12345
```
Make request to the webserver on defined port
```bash
$curl http://localhost:12345/index.html -v
*   Trying 127.0.0.1...
* Connected to localhost (127.0.0.1) port 12345 (#0)
> GET /index.html HTTP/1.1
> Host: localhost:12345
> User-Agent: curl/7.64.1
> Accept: */*
>
< HTTP/1.1 200 OK
< content-length: 108
< Connection: close
< content-type: text/html
< date: Thu, 27 Aug 2020 14:30:11 GMT
< server: Http Server / 1.0

$curl http://localhost:12345/random.html -v
* Connected to localhost (127.0.0.1) port 12345 (#0)
> GET /random.html HTTP/1.1
> Host: localhost:12345
> User-Agent: curl/7.64.1
> Accept: */*
>
< HTTP/1.1 404 Not Found
< content-length: 78
< Connection: close
< content-type: text/html
< date: Thu, 27 Aug 2020 14:30:33 GMT
< server: Http Server / 1.0
```
The server requests & responses are logged to the console output
```bash
Thu, 27 Aug 2020 14:29:54 GMT GET / HTTP/1.1 200 -
Thu, 27 Aug 2020 14:30:11 GMT GET /index.html HTTP/1.1 200 -
Thu, 27 Aug 2020 14:30:33 GMT GET /random.html HTTP/1.1 404 -
```