// Writted by hwpoison
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include <winsock2.h>

#define BUFFER_SIZE 1024
#define MAX_PATH_LENGTH 400
#define MAX_MIME_TYPES 30

typedef struct {
    const char *extension;
    const char *mime_type;
} MimeType;

// utils functions
char* get_current_datetime();
const char *get_filename_extension(const char* file_path);
const char *get_filename_mimetype(const char *path);
void remove_slash_from_start(char* str);
size_t get_file_length(FILE *file);

void print(const char* msg, ...);


// response functions
void send_404_response(SOCKET socket);
void send_500_response(SOCKET socket);
void send_200_http_response(FILE *file, SOCKET socket, const char *content_type, size_t content_length);
void send_206_http_response(FILE *file, SOCKET socket, const char *content_type, size_t file_size, size_t start, size_t end);
void stream_file_content(FILE *file, SOCKET socket);

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    SOCKET server_socket, new_client_socket;

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int port;

    char buffer[BUFFER_SIZE] = {0};
    char *input_filename, *file_to_serve_path;
    size_t start_offset, end_offset;
    size_t file_size;
    char tmpbuff[BUFFER_SIZE] = {0};
    

    if (argc < 2 || argc == 1) {
        printf("Usage: %s <port> <file_name.html>\n\
        If file name is not specified, all content of the current dir will be served.\n", argv[0]);
        return 1;
    }
    printf("-> Welcome to tinyC!\n");

    // extract args
    input_filename = argv[2];
    port = atoi(argv[1]);

    if(input_filename!=NULL){
      print("[+] Serving only %s file.", input_filename);
    }

    // winsock init
    print("Initializing server socket.");
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        perror("Error with winsock");
        exit(EXIT_FAILURE);
    }

    // create the socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        perror("Error to create the socket");
        exit(EXIT_FAILURE);
    }

    // setup the socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // bind addr and port
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        perror("[x] Error binding the socket to address and port.");
        exit(EXIT_FAILURE);
    }

    // listen incoming connections
    if (listen(server_socket, 3) == SOCKET_ERROR) {
        perror("Error to listen.");
        exit(EXIT_FAILURE);
    }
    print("Listening on :%d port...", port);

    // accept incoming connections and handle it (1 per time)
    while (1) {
        // the client make a request to the server and a socket connection is created and stablished
        if ((new_client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen)) == INVALID_SOCKET) {
            perror("[x] Error accepting the connection");
            exit(EXIT_FAILURE);
        }

        // get client ip address
        char *ip = inet_ntoa(address.sin_addr);
        print("Petition incoming  from %s", ip);

        // Read the HTTP request content
        recv(new_client_socket, buffer, BUFFER_SIZE, 0);
        print("Client Request:\n %s\n== End of Client request ==", buffer);


        // If file to serve was not specified, serve all dir content
        strcpy(tmpbuff, buffer);
        if(input_filename == NULL){
          // Extract the uri content (ex: /image.jpg)
          char *path_start = strchr(tmpbuff, ' ') + 1; 
          char *path_end = strchr(path_start, ' ');  
          
          if (path_start != NULL && path_end != NULL) {
              *path_end = '\0';
              file_to_serve_path = path_start;
              print("URL path: %s", file_to_serve_path);
          }
          // for get file relative path ( '/index.html' > 'index.html')
          remove_slash_from_start(file_to_serve_path); 
        }else{
          // serve only specified file.
          file_to_serve_path = input_filename;
        }

        // Open the file in the same executable path
        print("Finding for %s file..", file_to_serve_path);
        FILE *file = fopen(file_to_serve_path, "rb");

        if (file == NULL) {
            send_404_response(new_client_socket);
        } else {
            file_size = get_file_length(file);
            start_offset = 0;
            end_offset = file_size - 1;

            // Check if the request is has a "Range" header and extract range to stream
            char* range_header = strstr(buffer, "Range: ");
            if (range_header != NULL) {
                sscanf(range_header, "Range: bytes=%I64d-%I64d", &start_offset, &end_offset);
                print("Range detected: from %I64d to %I64d", start_offset, end_offset);
                send_206_http_response(
                    file, 
                    new_client_socket, 
                    get_filename_mimetype(file_to_serve_path), 
                    file_size,
                    start_offset, 
                    end_offset);
            }else{
                send_200_http_response(
                    file,
                    new_client_socket,
                    get_filename_mimetype(file_to_serve_path),
                    file_size);
            }
          }
        // close client socket connection 
        closesocket(new_client_socket);
    }
    // close server socket and release memory
    closesocket(server_socket);
    WSACleanup();
    return 0;
}

// all supported mimetypes
MimeType mime_types[MAX_MIME_TYPES] = {
    { ".html", "text/html" },
    { ".htm", "text/html" },
    { ".txt", "text/plain" },
    { ".css", "text/css" },
    { ".js", "application/javascript" },
    { ".json", "application/json" },
    { ".xml", "application/xml" },
    { ".gif", "image/gif" },
    { ".jpeg", "image/jpeg" },
    { ".mkv",  "video/mp4"},
    { ".flac",  "audio/flac"},
    { ".mp3",  "audio/mp3"},
    { ".jpg", "image/jpeg" },
    { ".png", "image/png" },
    { ".svg", "image/svg+xml" },
    { ".mp4", "video/mp4" },
    { ".ico", "image/x-icon" },
    { ".pdf", "application/pdf" },
    { ".doc", "application/msword" },
    { ".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document" },
    { ".xls", "application/vnd.ms-excel" },
    { ".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet" },
    { ".ppt", "application/vnd.ms-powerpoint" },
    { ".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation" }
};

const char *not_found_response =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: %d\r\n\r\n<html>"
    "<head><title> Oops! 404 Not Found</title></head>"
    "<body><h1>404 Not Found! :(</h1>"
    "<p>The requested resource was not found on this server.</p>"
    "</body></html>";

const char *server_side_error =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: %d\r\n\r\n<html>"
    "<head><title>500 Internal Error</title></head>"
    "<body><h1>500</h1><p>Internal server error.</p>"
    "</body></html>";

void print(const char* msg, ...) {
    va_list args;
    va_start(args, msg);

    printf("[%s] ", get_current_datetime());
    vprintf(msg, args);
    printf("\n");

    va_end(args);
}

size_t get_file_length(FILE *file){
    fseek(file, 0, SEEK_END);
    size_t fileLength = ftell(file);
    fseek(file, 0, SEEK_SET);
    return fileLength;
}

void stream_file_content(FILE *file, SOCKET socket){
    char buffer[BUFFER_SIZE];
    size_t bytesRead;
    // stream file content
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (send(socket, buffer, bytesRead, 0) == -1) {
            break; 
        }   
    }
}

void remove_slash_from_start(char* str) {
    size_t length = strlen(str);
    if (length > 0 && str[0] == '/') {
        memmove(str, str + 1, length);
        str[length - 1] = '\0';
    }
}

const char *get_filename_extension(const char *path) {
    const char *extension = strrchr(path, '.');
    return extension!=NULL && extension!=path?extension:"";
}

const char *get_filename_mimetype(const char *path) {
    const char *extension = get_filename_extension(path);
    for (int i = 0; mime_types[i].extension != NULL; i++) {
        if (strcmp(mime_types[i].extension, extension) == 0) { // improve this shit!
            return mime_types[i].mime_type;
        }
    }
    return "application/octet-stream"; // default mimetype
}

void send_404_response(SOCKET socket) {
    char response[1024];
    sprintf(response, not_found_response, strlen(not_found_response));
    send(socket, response, strlen(response), 0);
    print("404 not found.");
}

void send_500_response(SOCKET socket) {
    char response[1024];
    sprintf(response, server_side_error, strlen(server_side_error));
    send(socket, response, strlen(response), 0);
    print("500 server side error.");
}

char* get_current_datetime() {
    time_t current_time;
    struct tm *local_time;
    current_time = time(NULL);
    local_time = localtime(&current_time);
    char* datetime = (char*) malloc(100 * sizeof(char));
    strftime(datetime, 100, "%Y-%m-%d %H:%M:%S", local_time);
    return datetime;
}

// normal request
void send_200_http_response(FILE *file, SOCKET socket, const char *content_type, size_t content_length) {
    // send header with range and content length (for video html stream content)
    char header[1024];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %I64d\r\n\r\n", content_type, content_length);
    send(socket, header, strlen(header), 0);
    stream_file_content(file, socket);

    print("[!] Done");
}

// for partial content
void send_206_http_response(FILE *file, SOCKET socket, const char *content_type, size_t file_size, size_t start, size_t end) {
    // seek the file to the specified rangue before send
    fseek(file, start, SEEK_SET);
    size_t contentLength = end - start + 1;

    // Check if the requested range is within the file size
    if (start > file_size || end > file_size) {
        print("[!] Error: requested range is out of bounds.");
        return;
    }

    // send header with range and content length (for video html stream content)
    char header[1024];
    sprintf(header, "HTTP/1.1 206 Partial Content\r\nContent-Type: %s\r\nContent-Range: bytes %I64d-%I64d/%I64d\r\nContent-Length: %I64d\r\n\r\n", 
        content_type, start, end, file_size, contentLength);
    send(socket, header, strlen(header), 0);
    stream_file_content(file, socket);

    printf("[!] Done at %s.\n", get_current_datetime());
}