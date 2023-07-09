// Writted by hwpoison
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>

#ifdef __linux__
    #include <sys/socket.h>
    #include <netinet/in.h>
    typedef int SocketType;
    #define SIZE_T_FORMAT "%zu"
    #define SEND_D_FLAG MSG_NOSIGNAL // avoid SIGPIPE signal
#else
    #include <winsock2.h>
    typedef SOCKET SocketType;
    #define SIZE_T_FORMAT "%Iu"
    #define SEND_D_FLAG 0
#endif

#define PRINT

#define MAX_HEADER_SIZE 1024
#define BUFFER_SIZE 5000 // 5kb
#define MAX_PATH_LENGTH 30
#define MAX_MIME_TYPES 30
#define DEFAULT_PORT 8081

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
void send_404_response(SocketType  socket);
void send_500_response(SocketType  socket);
void send_response(SocketType socket, const char *response_content);
void send_200_http_response(FILE *file, SocketType  socket, const char *content_type, size_t content_length);
void send_206_http_response(FILE *file, SocketType  socket, const char *content_type, size_t file_size, size_t start, size_t end);
void send_file_in_chuncks(FILE *file, SocketType  socket);
void close_socket(SocketType socket);

char* get_arg_value(int argc, char **argv, char *target_arg){
    if(argc>=255) 
        return NULL;
    for(int arg_idx = 0; arg_idx < argc; arg_idx++){
        if(!strcmp(argv[arg_idx], target_arg))
            return argv[arg_idx+1]==NULL?"":argv[arg_idx+1];
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = 0;
    char buffer[BUFFER_SIZE] = {0};
    char tmpbuffer[BUFFER_SIZE] = {0};
    char *input_port, *input_filename, *file_to_serve_path, *client_ip;
    size_t file_size, start_offset, end_offset;

    // Socket vars declaration
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    SocketType server_socket, new_client_socket;
    #ifdef _WIN32
        WSADATA wsaData;
    #endif

    if(get_arg_value(argc, argv, "--help") != NULL){
        printf("-------- TinyC server (by hwpoison) -------- \n"
            "Usage: %s --port <port> --file <file_name.html>\n"
            " ex: tinyc.exe --port 3543 --file index.html\n"
            " - Default port is %d\n"
            " - If file is doesn't specified, will serve all dir content.\n", argv[0], DEFAULT_PORT);
        return 0;
    }

    print("####  Welcome to tinyC!  ####");
    
    // get port from args
    if((input_port= get_arg_value(argc, argv, "--port")) == NULL)
        port = DEFAULT_PORT;
    else
        port = atoi(input_port);

    // get filename from args
    input_filename = get_arg_value(argc, argv, "--file");
    if(input_filename != NULL){
      print("[+] Serving only %s file.", input_filename);
    }

    #ifdef _WIN32
        // winsock init
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            perror("Error with winsock");
            exit(EXIT_FAILURE);
        }
    #endif

    print("Initializing server socket.");
    // create server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    // set up the socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // bind addr and port
    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        print("[x] Error binding the socket to address and port %d.", port);
        perror("Caused by ");
        exit(EXIT_FAILURE);
    }

    // start to listen incoming connections
    if (listen(server_socket, 3) < 0) {
        perror("Error to listen connections.");
        exit(EXIT_FAILURE);
    }
    print("Listening on :%d port...", port);

    // accept incoming connections and handle it (1 per time)
    for(;;) {
        // the client make a request to the server and a socket connection is created and stablished
        #ifdef __linux__
            if ((new_client_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        #else
            if ((new_client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen)) == INVALID_SOCKET) {
        #endif
                perror("[x] Error accepting the connection");
                exit(EXIT_FAILURE);
            }

        // Read the HTTP request content
        #ifdef __linux__
            read(new_client_socket, buffer, BUFFER_SIZE);
            client_ip =":";
        #else
            recv(new_client_socket, buffer, BUFFER_SIZE, 0);
            client_ip = inet_ntoa(address.sin_addr);
        #endif

        print(" ## Petition incoming  from %s", client_ip);
        print("\n == Client Request content ==\n %s\n== End of Client request ==\n", buffer);
        strcpy(tmpbuffer, buffer);
        // If file to serve was not specified, serve all dir content
        if(input_filename == NULL){
          // Extract the uri content from request content (ex: /image.jpg)
          char *path_start = strchr(tmpbuffer, ' ') + 1; // always are in the start of request. 
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
            print("Size:"SIZE_T_FORMAT, file_size);
            // Check if the request is has a "Range" header and extract range to stream
            char* range_header = strstr(buffer, "Range: bytes=");
            if (range_header != NULL) {
                sscanf(range_header, "Range: bytes="SIZE_T_FORMAT"-"SIZE_T_FORMAT"", &start_offset, &end_offset);
                print("Range detected: from "SIZE_T_FORMAT" to " SIZE_T_FORMAT, start_offset, end_offset);
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
            fclose(file);
          }
        // close client socket connection 
        close_socket(new_client_socket);
    }
    // close server socket and release memory
    close_socket(server_socket);
    #ifdef _WIN32
        WSACleanup();
    #endif
    return 0;
}

void close_socket(SocketType socket) {
    #ifdef __linux__
        close(socket);
    #else 
        closesocket(socket);
    #endif
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
    { ".mkv",  "video/x-matroska"},
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

const char *server_side_error_response =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: %d\r\n\r\n<html>"
    "<head><title>500 Internal Error</title></head>"
    "<body><h1>500</h1><p>Internal server error.</p>"
    "</body></html>";

void print(const char* msg, ...) {
    #ifdef PRINT
        va_list args;
        va_start(args, msg);

        printf("[%s] ", get_current_datetime());
        vprintf(msg, args);
        printf("\n");

        va_end(args);
    #endif
}

size_t get_file_length(FILE *file){
    fseek(file, 0, SEEK_END);
    size_t fileLength = ftell(file);
    fseek(file, 0, SEEK_SET);
    return fileLength;
}

void send_file_in_chuncks(FILE *file, SocketType  to_socket){
    char buffer[BUFFER_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if(send(to_socket, buffer, bytesRead, SEND_D_FLAG ) == -1){
            perror("File send interrupted:");
            close_socket(to_socket);
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
        if (strcmp(mime_types[i].extension, extension) == 0)
            return mime_types[i].mime_type;
    }
    return "application/octet-stream"; // default mimetype
}

void send_response(SocketType socket, const char *response_content) {
    send(socket, response_content, strlen(response_content), 0);
}

void send_404_response(SocketType  socket) {
    send_response(socket, not_found_response);
    print("404 not found.");
}

void send_500_response(SocketType  socket) {
    send_response(socket, server_side_error_response);
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

// send header with range and content length
void send_200_http_response(FILE *file, SocketType  socket, const char *content_type, size_t content_length) {
    char header[MAX_HEADER_SIZE];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: " SIZE_T_FORMAT "\r\n\r\n", content_type, content_length);
    send_response(socket, header);
    print("Response header: %s", header );
    send_file_in_chuncks(file, socket);
    print("Response Done.\n");
}

// response of partial content that use content range for send binary content from x to y offset.
void send_206_http_response(FILE *file, SocketType  socket, const char *content_type, size_t file_size, size_t start, size_t end) {
    // seek the file to the specified rangue before send
    fseek(file, start, SEEK_SET);
    size_t contentLength = end - start + 1;

    // Check if the requested range is within the file size
    if (start > file_size || end > file_size) {
        print("[!] Error: requested range is out of bounds.");
        return;
    }

    // send header with range and content length (for video html stream content)
    char header[MAX_HEADER_SIZE];
    sprintf(header, "HTTP/1.1 206 Partial Content\r\nContent-Type: %s\r\nContent-Range: bytes " SIZE_T_FORMAT "-"SIZE_T_FORMAT"/"SIZE_T_FORMAT"\r\nContent-Length: "SIZE_T_FORMAT"\r\n\r\n", 
        content_type, start, end, file_size, contentLength);
    send_response(socket, header);
    send_file_in_chuncks(file, socket);
    print("Response done.\n");
}