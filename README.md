## TinyC http-server

#### A very basic HTTP server in C

Is a multithread lightweight web server written in pure C. Compilable for Windows and Linux for server static content and media files from a directory.

![](https://upload.wikimedia.org/wikipedia/commons/thumb/5/5b/HTTP_logo.svg/320px-HTTP_logo.svg.png)

![](https://i.ibb.co/5Mx1WrD/tinyc.png)

## Basic usage

Usage:

```plaintext
tinyc.exe --port <port> --folder <folder_path>
```

Example:

```plaintext
tinyc.exe --port 5656 --folder /simple_web --backlog 4 --no-print
```

For help usage:

```plaintext
tinyc.exe --help
```

## Options

```plaintext
Basic usage: tinyc --port 8081 --folder /my_web
 example: tinyc --port 3543 --folder simple_web/index.html

Options:
        --folder <folder_path>: Folder to serve. By default is a relative path due to executable location.
        --ip: Set server IP. Default: ANY (Local/Network).
        --port <port_number>: Port number. Default is 8081
        --backlog <number>: Max server listener.
        --max-threads <number>: Max server threads.
        --default-redirect <file_path>/: redirect / to default file route. ex: simple_web/index.html
        --no-logs: No print log (Less I/O bound due to stdout and less memory consumption)).
        --no-file-explorer: Disable file explorer.
```

*   If you dont specify any args, servers will run on localhost:8081 by default serving executable location content.

## How to build

Has two versions, default multithread (all) using pthread and monothread using nothing (monothread).

```plaintext
make all
make monothread
```

## **Tested on**

<table><tbody><tr><td>Windows</td><td>GCC</td><td>gcc (x86_64-posix-seh-rev1, Built by MinGW-Builds project) 13.1.0</td></tr><tr><td>Linux</td><td>GCC</td><td>gcc (Ubuntu 9.4.0-1ubuntu1~20.04.1) 9.4.0</td></tr></tbody></table>

**TODO: add utf-8 support for Windows**