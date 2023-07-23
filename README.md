# TinyC http-server
#### A very basic HTTP server in C
<center>
Is a lightweight web server written in C using sockets. Compilable for Windows and Linux for server static content and media files.


![](https://upload.wikimedia.org/wikipedia/commons/thumb/5/5b/HTTP_logo.svg/320px-HTTP_logo.svg.png)
</center>

## Basic usage
Usage:
```sha
tinyc.exe --port <port> --folder <folder_path>
```
Example:
```sh
tinyc.exe --port 5656 --folder /simple_web --backlog 4 --no-print
```
For help usage:
```sh
tinyc.exe --help
```

## Options
```sh
Basic usage: tinyc --port 8081 --folder /my_web
 example: tinyc --port 3543 --folder simple_web/index.html

Options:
        --folder <folder_path>: Folder to serve. By default serve all executable location dir content.
        --port <port_number>: Port number. Default is 8081
        --backlog <number>: Max server listener.
        --max-threads <number>: Max server threads.
        --default-redirect <file_path>/: redirect / to default file route. ex: simple_web/index.html
        --no-print : No print log (less consumption).
        --no-file-explorer: Disable file explorer.
```
* If you dont specify any args, servers will run on localhost:8081 by default serving executable location content.

## How to build
Has two versions, default multithread (all) using pthread and monothread using nothing (monothread).
```sh
make all
make monothread
```

**TODO: add utf-8 support**