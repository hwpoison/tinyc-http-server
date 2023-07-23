#ifndef TINYC_H
#define TINYC_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>

// Set multithread mode
#ifdef MULTITHREAD_ON
    #include <pthread.h>
#endif

#ifdef __linux__
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <sys/time.h>
    #include <arpa/inet.h>
    #include <dirent.h>

    typedef int32_t SocketType;
    #define SIZE_T_FORMAT "%zu"
    #define SEND_D_FLAG MSG_NOSIGNAL // avoid SIGPIPE signal
#else
    #include <winsock2.h>
    #include <windows.h>

    typedef SOCKET SocketType;
    #define SIZE_T_FORMAT "%Iu"
    #define SEND_D_FLAG 0
#endif

#define TRUE  1
#define FALSE 0

#define MAX_HEADER_SIZE 1024
#define BUFFER_SIZE 10000 // 10kb
#define MAX_PATH_LENGTH 30
#define MAX_THREADS 250
#define MAX_MIME_TYPES 30
#define DEFAULT_PORT 8081 // server default server
#define SERVER_BACKLOG 250 // server max listen connections
#define CLIENT_TIMEOUT 15
#define EXPLORER_MAX_FILES 200
#define EXPLORER_MAX_FILENAME_SIZE 200

int8_t print_all = TRUE; // show all server output 

typedef struct {
    const char *extension;
    const char *mime_type;
} MimeType;

typedef struct {
    SocketType socket;
    char *default_route;
    char *folder_to_serve;
    int8_t show_explorer;
} connection_params;

#ifdef MULTITHREAD_ON
    void *handle_connection_thread(void *thread_args);
#endif

// Utils functions
void print(const char* msg, ...);
char* get_current_datetime();
const char *get_filename_extension(const char* file_path);
const char *get_filename_mimetype(const char *path);
void remove_slash_from_start(char* str);
int starts_with(const char *str, const char *word);
size_t get_file_length(const char* filename);
char* get_arg_value(int argc, char **argv, char *target_arg);
char *extract_URI_from_header(char *header_content);
void* safe_malloc(size_t size);
char **get_list_file(const char* path, size_t *file_amount);
void normalize_path(char* str);
void concatenate_string(char** str, const char* new_str);

// Response functions
void send_response(const char *response_content, SocketType socket);
void send_404_response(SocketType  socket); //  not found
void send_500_response(SocketType  socket); // internal error
void send_302_response(SocketType  socket, char *uri) ; // redirection
void send_200_http_response(FILE *file, SocketType  socket, const char *content_type, size_t content_length);
void send_206_http_response(FILE *file, SocketType  socket, const char *content_type, size_t file_size, size_t start, size_t end);
void send_file_in_chuncks(FILE *file, SocketType  socket);
void close_socket(SocketType socket);
void handle_connection(connection_params *params);

// All supported mimetypes
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
    { ".pdf", "application/pdf" }
};

// File explorer
const char *FILE_EXPLORER_HEADER = "HTTP/1.1\r\n"
"Content-Type: text/html\r\n\r\n"
"<!DOCTYPE html>"
"<html>"
"<head><title>TinyC</title></head>"
"<body>"
"<h2>Content into: %s</h2>"
"<hr>"
"<ul style='padding-left:3em'>";

const char *FILE_EXPLORER_LIST_ELEMENT = "<a href='%s'>%s</a><br>";

const char *FILE_EXPLORER_FOOTER = "</ul>"
"<hr>"
"</body>"
"</html>";

// HTTP common responses
const char *HTTP_404_NOT_FOUND =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: %d\r\n\r\n<html>"
    "<head><title> Oops! 404 Not Found</title></head>"
    "<body><h1>404 Not Found! :(</h1>"
    "<p>The requested resource was not found on this server.</p>"
    "</body></html>";

const char *HTTP_500_INTERNAL_ERROR =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: %d\r\n\r\n<html>"
    "<head><title>500 Internal Error</title></head>"
    "<body><h1>500</h1><p>Internal server error.</p>"
    "</body></html>";

const char *HTTP_302_REDIRECTION = 
    "HTTP/1.1 302 Found\r\n"
    "Location: %s\r\n"
    "Connection: close\r\n"
    "\r\n";
#endif