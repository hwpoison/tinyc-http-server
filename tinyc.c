// Writted by hwpoison
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 4048
#define MAX_PATH_LENGTH 400
#define MAX_MIME_TYPES 30

typedef struct {
    const char *extension;
    const char *mime_type;
} MimeType;

const char *get_filename_extension(const char*file_path);
const char *get_filename_mimetype(const char *path);
void remove_slash_from_start(char* str);
void send_404_response(SOCKET socket);
char* get_current_datetime();
void send_http_response(
  SOCKET socket, 
  int status, 
  const char *content_type, 
  const char *content, 
  size_t content_length ); 


int main(int argc, char *argv[]) {
    WSADATA wsaData;
    SOCKET server_socket, new_client_socket;

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int port;

    char buffer[BUFFER_SIZE] = {0};
    char *input_filename, *file_to_serve_path;
    uint8_t *response_body = NULL;
    FILE *file;

    if (argc < 2 || argc == 1) {
        printf("Usage: %s <port> <file_name.html>\n\
        If file name is not specified, all content of the current dir will be served.\n", argv[0]);
        return 1;
    }

    // extract args
    input_filename = argv[2];
    port = atoi(argv[1]);

    if(input_filename!=NULL){
      printf("[+] Serving only %s file.\n", input_filename);
    }

    // winsock init
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
    printf("[!] Listening by :%d port...\n", port);

    // accept the connection and handle it
    while (1) {
        // the client make a request to serve and a socket is opened and ready for information exchange
        if ((new_client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen)) == INVALID_SOCKET) {
            perror("[x] Error accepting the connection");
            exit(EXIT_FAILURE);
        }
        // Obtain the client IP address
        char *ip = inet_ntoa(address.sin_addr);
        printf("[>] %s from %s\n", get_current_datetime(), ip);
        // Read the HTTP request content
        recv(new_client_socket, buffer, BUFFER_SIZE, 0);

        // If file to serve was not specified, serve all dir content
        if(input_filename == NULL){
          // Extract the uri content (ex: /image.jpg)
          char *path_start = strchr(buffer, ' ') + 1; 
          char *path_end = strchr(path_start, ' ');  
          
          if (path_start != NULL && path_end != NULL) {
              *path_end = '\0';
              file_to_serve_path = path_start;
              printf("[!] URL path: %s\n", file_to_serve_path);
          }
          remove_slash_from_start(file_to_serve_path);
        }else{
          file_to_serve_path = input_filename;
        }

        // Open the file in the same executable path
        printf("[*] Finding for %s file..\n", file_to_serve_path);
        file = fopen(file_to_serve_path, "rb");
        
        if (file == NULL) {
            send_404_response(new_client_socket);
        } else {
            // get file size
            fseek(file, 0, SEEK_END);
            long file_size = ftell(file);
            fseek(file, 0, SEEK_SET);
            printf("[!] File lenght:%ld\n", file_size);

            // allocate memory for the body response
            response_body = malloc(file_size + 1);
            if (response_body == NULL) {
                perror("[x] Error allocating memory");
                exit(EXIT_FAILURE);
            }

            // read and close the file
            size_t bytes_read = fread(response_body, 1, file_size, file);
            if(bytes_read!=file_size){
              perror("[x] Failed reading the file!");
            }

            fclose(file);

            // add the EOF to the body content
            response_body[file_size] = '\0';

            send_http_response(
              new_client_socket, 
              200, 
              get_filename_mimetype(file_to_serve_path), 
              (const char *)response_body, 
              file_size);
            free(response_body);
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
    { ".jpg", "image/jpeg" },
    { ".png", "image/png" },
    { ".svg", "image/svg+xml" },
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
    "Content-Length: %d\r\n"
    "\r\n"
    "<html>"
    "<head><title>Oops! 404 Not Found</title></head>"
    "<body>"
    "<h1>404 Not Found :(</h1>"
    "<p>The requested resource was not found on this server.</p>"
    "</body>"
    "</html>";

void remove_slash_from_start(char* str) {
    size_t length = strlen(str);
    if (length > 0 && str[0] == '/') {
        memmove(str, str + 1, length);
        str[length - 1] = '\0';
    }
}

const char *get_filename_extension(const char *path) {
    const char *extension = strrchr(path, '.');
    if (extension != NULL && extension != path) {
        return extension;
    }
    return "";
}

const char *get_filename_mimetype(const char *path) {
    const char *extension = get_filename_extension(path);
    for (int i = 0; i < MAX_MIME_TYPES; i++) {
        if (strcmp(mime_types[i].extension, extension) == 0) {
            return mime_types[i].mime_type;
        }
    }
    return "application/octet-stream"; // default mimetype
}

void send_404_response(SOCKET socket) {
    char response[1024];
    sprintf(response, not_found_response, strlen(not_found_response));
    send(socket, response, strlen(response), 0);
    printf("[?] 404 not found.");
}

void send_http_response(
  SOCKET socket, 
  int status, 
  const char *content_type, 
  const char *content, 
  size_t content_length ) 
{
    printf("[*] Sending response.\n");
    char header[1024];
    sprintf(header, "HTTP/1.1 %d OK\r\nContent-Type: %s\r\nContent-Length: %I64d\r\n\r\n", status, content_type, content_length);
    send(socket, header, strlen(header), 0);
    send(socket, content, content_length, 0);
    printf("[!] Response done.\n\n");
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
