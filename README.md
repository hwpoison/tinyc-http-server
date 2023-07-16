# TinyC http-server
#### A very basic HTTP server in C
<center>
Is a lightweight web server written in C using sockets. Compilable for Windows and Linux for server static content and media files.

![](https://upload.wikimedia.org/wikipedia/commons/thumb/5/5b/HTTP_logo.svg/320px-HTTP_logo.svg.png)
</center>

## Basic usage
Usage:
```sh
tinyc.exe --port <port> --file <file_name>
```
Example:
```sh
tinyc.exe --port 5656 --folder /simple_web --backlog 4
```
For help usage:
```sh
tinyc.exe --help
```

* The port and file are optional, if the file is not specified, the webserver will serve all content into the executable path.

## How to build
Has two versions, one monothread and multithreading using pthread.
```sh
make mono 
make multithread
```