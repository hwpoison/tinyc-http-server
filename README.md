# TinyC http server
#### A very simple HTTP server written in C is a lightweight web server written in C, 
Is a lightweight single file web server written in C. Compilable for Windows and Linux

## Basic usage
```sh
windows: tinyc.exe <port> <file.html>
linux: ./tinyc <port> <file.html>
```
The file is optional, if is not specified, the webserver will serve all content into the executable path.

## How to build
```sh
make all
```