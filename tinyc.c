// Writted by hwpoison (15/7/2023)
#include "tinyc.h"

int main(int argc, char *argv[]) {
    char *input_arg = NULL;
    char *folder_to_serve = NULL, *default_route = NULL;
    
    char client_ip[8] = ":";

    int16_t port = DEFAULT_PORT;
    int16_t backlog = SERVER_BACKLOG;

    // Socket vars declaration
    SocketType server_socket, client_socket;
    struct sockaddr_in address;
    int32_t addrlen = sizeof(address);

    #ifdef _WIN32
        WSADATA wsaData;
    #endif

    /* =====================================  */
    /* ======= Arg parse ===================  */
    /* =====================================  */
        
    if(get_arg_value(argc, argv, "--help") != NULL){
        printf(
            #ifdef MULTITHREAD_ON
                "::: TinyC http multithread server (by hwpoison) :::\n"
            #else
                "::: TinyC http non-thread server (by hwpoison) :::\n"
            #endif
            "\nBasic usage: %s --port 8081 --folder /my_web\n"
            " example: %s --port 3543 --folder simple_web/index.html\n"
            "\nOptions:\n"
            "\t--folder <folder_path>: Folder to serve. By default serve all executable location dir content.\n"
            "\t--port <port_number>: Port number. Default is %d\n"
            "\t--backlog <number>: Max server listener.\n"
            "\t--default-redirect <file_path>: redirect / to default file route.\n"
            "\t--no-print : No print log (less consumption).\n"
            ,argv[0], argv[0], DEFAULT_PORT);
        return 0;
    }

    // Get args
    if((input_arg = get_arg_value(argc, argv, "--port")) != NULL)
        port = atoi(input_arg);

    if((input_arg = get_arg_value(argc, argv, "--backlog")) != NULL)
        backlog = atoi(input_arg);

    if((get_arg_value(argc, argv, "--no-print")) != NULL)
        print_all = 0;

    if((input_arg = get_arg_value(argc, argv, "--folder")) != NULL)
        folder_to_serve = input_arg;

    default_route = get_arg_value(argc, argv, "--default-redirect");

    print("####  Welcome to tinyC!  ####");

    #ifdef MULTITHREAD_ON
        pthread_t active_threads[MAX_THREADS] = {-1};
        int16_t thread_count = 0;
        print("Multithreading enabled.");
    #endif

    /* =====================================  */
    /* ======= Socket initialization =======  */
    /* =====================================  */
    
    print("Initializing server socket.");
    // winsock init
    #ifdef _WIN32
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            perror("Error with winsock");
            exit(EXIT_FAILURE);
        }
    #endif

    // Create server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    // Set up the socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    struct timeval timeout = { .tv_sec = CLIENT_TIMEOUT, .tv_usec = 0};

    // Bind addr and port
    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        print("[x] Error binding the socket to address and port %d.", port);
        perror("Caused by ");
        exit(EXIT_FAILURE);
    }

    // Start to listen incoming connections
    if (listen(server_socket, backlog) < 0) {
        perror("Error to listen connections.");
        exit(EXIT_FAILURE);
    }
    print("Listening on :%d port...", port);

    /* =====================================  */
    /* ======= Accept connections loop =====  */
    /* =====================================  */
    for(;;) {
        #ifdef MULTITHREAD_ON
        // Check threads limits
        if(thread_count>=MAX_THREADS){
            print("Server too busy...");
            for(int i = 0; i < MAX_THREADS; i++){
                pthread_join(active_threads[i], NULL);
            }
            thread_count = 0;
            print("All threads free up.");
        }
        #endif

        #ifdef __linux__
            if ((client_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                strcpy(client_ip, ":");
        #else
            if ((client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen)) == INVALID_SOCKET) {
                strcpy(client_ip, inet_ntoa(address.sin_addr));
        #endif
                perror("[x] Error accepting the connection");
                exit(EXIT_FAILURE);
        }

        if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) == -1) {
            perror("Error to set up timeput on client socket");
            close(client_socket);
            continue;
        }

        // Create new thread for handle connection
        print("Incoming connection from %s  - (fp:%d)", client_ip, client_socket);

        #ifdef MULTITHREAD_ON
            client_handle *thread_args = malloc(sizeof(client_handle));
            thread_args->socket = client_socket;
            thread_args->default_route = default_route;
            thread_args->folder_to_serve = folder_to_serve;

            pthread_create(&active_threads[thread_count], NULL, handle_connection_thread, (void*)thread_args);
            pthread_detach(active_threads[thread_count]);
            thread_count++;
        #else
            handle_connection(client_socket, default_route, folder_to_serve);
        #endif
    }   

    // Close server socket and release memory
    close_socket(server_socket);
    #ifdef _WIN32
        WSACleanup();
    #endif
    return 0;
}

void print(const char* msg, ...) {
    if(print_all){
        va_list args;
        char *full_date = get_current_datetime();
        va_start(args, msg);
        printf("[%s] ", full_date);
        vprintf(msg, args);
        printf("\n");
        va_end(args);
        free(full_date);
    }
}

void send_206_http_response(FILE *file, SocketType  socket, const char *content_type, size_t file_size, size_t start, size_t end) {
    // Seek the file to the specified rangue before send
    fseek(file, start, SEEK_SET);

    // Check if the requested range is within the file size
    if (start > file_size || end > file_size) {
        print("[!] Error: requested range is out of bounds.");
        send_500_response(socket);
        return;
    }

    // Send header with range and content length (for video html stream content)
    char header[MAX_HEADER_SIZE];
    sprintf(header, "HTTP/1.1 206 Partial Content\r\n"
                    "Connection: keep-alive\r\n"
                    "Accept-Ranges: bytes\r\n"
                    "Content-Type: %s\r\n"
                    "Keep-Alive: timeout=5, max=200\r\n"
                    "Content-Range: bytes " SIZE_T_FORMAT "-" SIZE_T_FORMAT "/" SIZE_T_FORMAT "\r\n"
                    "Content-Length: " SIZE_T_FORMAT "\r\n\r\n", content_type, start, end, file_size, end - start + 1);
    send_response(header, socket);
    // Send the file chunks
    send_file_in_chuncks(file, socket);
    print("Response 206 done.");
}

// send header with range and content length
void send_200_http_response(FILE *file, SocketType  socket, const char *content_type, size_t content_length) {
    char header[MAX_HEADER_SIZE];
    sprintf(header, "HTTP/1.1 200 OK\r\n"
                    "Connection: keep-alive\r\n"
                    "Content-Type: %s\r\n"
                    "Keep-Alive: timeout=2, max=200\r\n"
                    "Accept-Ranges: bytes\r\n"
                    "Content-Length: " SIZE_T_FORMAT "\r\n\r\n", content_type, content_length);
    send_response(header, socket);
    send_file_in_chuncks(file, socket);
    print("Response 200 Done.");
}

char *get_arg_value(int argc, char **argv, char *target_arg){
    for(int arg_idx = 0; arg_idx < argc; arg_idx++){
        if(!strcmp(argv[arg_idx], target_arg)) // <arg> <value> 
            return argv[arg_idx+1]==NULL?"":argv[arg_idx+1];
    }
    return NULL;
}

int starts_with(const char *str, const char *word) {
    size_t word_len = strlen(word);
    return strncmp(str, word, word_len) == 0;
}

void send_302_response(SocketType  socket, char *uri) {
    char buffer[BUFFER_SIZE];
    sprintf(buffer, redir_302, uri);
    send_response(buffer, socket);
    print("302 redirection to %s", uri);
}

size_t get_file_length(FILE *file){
    fseek(file, 0, SEEK_END);
    size_t fileLength = ftell(file);
    fseek(file, 0, SEEK_SET);
    return fileLength;
}

void send_response(const char *response_content, SocketType to_socket) {
    send(to_socket, response_content, strlen(response_content), 0);
}

void send_file_in_chuncks(FILE *file, SocketType to_socket){
    char buffer[BUFFER_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if(send(to_socket, buffer, bytesRead, SEND_D_FLAG ) < 0)
            break;
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

void send_404_response(SocketType  socket) {
    send_response(not_found_response, socket);
    print("404 not found.");
}

void send_500_response(SocketType  socket) {
    send_response(server_side_error_response, socket);
    print("500 server side error.");
}

char* get_current_datetime() {
    time_t current_time;
    struct tm *local_time;
    current_time = time(NULL);
    local_time = localtime(&current_time);
    char* datetime = (char*) malloc(100 * sizeof(datetime));
    strftime(datetime, 100, "%Y-%m-%d %H:%M:%S", local_time);
    return datetime;
}

void close_socket(SocketType socket) {
    print("Closing (fp:%d) connection.", socket);
    #ifdef __linux__
        close(socket);
    #else 
        closesocket(socket);
    #endif
}

void handle_connection(SocketType client_socket, char *default_route, char *folder_to_serve){
    char buffer[BUFFER_SIZE] = {0};
    char buffer_copy[BUFFER_SIZE] = {0};

    char *file_to_serve_path = NULL;

    int8_t in_folder = 0;
    size_t read_bytes, file_size, start_offset, end_offset;

     for(;;){ // Read-Send between Server-Client loop
        print("Reading from client...");
        #ifdef __linux__
            read_bytes = read(client_socket, buffer, BUFFER_SIZE);
        #else
            read_bytes = recv(client_socket, buffer, BUFFER_SIZE, 0);
        #endif
        
        if (read_bytes == 0) {
            print("Connection closed by client");
            close_socket(client_socket);
            break;
        } else if (read_bytes == -1) {
            print("Error reading from socket content (fp:%d)", client_socket);
            close_socket(client_socket);
            break;
        }
        
        // Extract the uri path content from request content (ex: /image.jpg)
        strcpy(buffer_copy, buffer);
        char *path_start = strchr(buffer_copy, ' ') + 1; // always are in the start of request. 
        char *path_end = strchr(path_start, ' ');  
        if (path_start != NULL && path_end != NULL) {
            *path_end = '\0';
            file_to_serve_path = path_start;
            print("URL path: %s", file_to_serve_path);
            remove_slash_from_start(file_to_serve_path); 
        }
        
        // Check if the path match with default folder
        if(folder_to_serve != NULL){
            remove_slash_from_start(folder_to_serve); 
            in_folder = !starts_with(file_to_serve_path, folder_to_serve);
        }

        print("Finding for %s file..", file_to_serve_path);
        FILE *file = fopen(file_to_serve_path, "rb");
        
        // Check if uri path == '/' and redirect to default route
        if(strcmp(file_to_serve_path, "") == 0 && default_route != NULL){
            print("Redirecting to %s", default_route);
            file_to_serve_path = default_route;
            send_302_response(client_socket, default_route);
        // If url path doesnt match with default folder, 404
        } else if (file == NULL || in_folder == 1) {
            send_404_response(client_socket);
        // Serve the file
        } else {
            file_size = get_file_length(file);
            start_offset = 0, end_offset = file_size -1;
            print("File size:"SIZE_T_FORMAT, file_size);

            // Check if the request is has a "Range" header and extract range to stream
            char* range_header = strstr(buffer, "Range: bytes=");
            if (range_header != NULL) {
                sscanf(range_header, "Range: bytes="SIZE_T_FORMAT"-"SIZE_T_FORMAT"", &start_offset, &end_offset);
                print("Range detected: from "SIZE_T_FORMAT" to " SIZE_T_FORMAT, start_offset, end_offset);
                send_206_http_response(
                    file, 
                    client_socket, 
                    get_filename_mimetype(file_to_serve_path), 
                    file_size,
                    start_offset, 
                    end_offset);
            }else{ 
                send_200_http_response(
                    file,
                    client_socket,
                    get_filename_mimetype(file_to_serve_path),
                    file_size);
            }
            fclose(file);
        }
    }
}

#ifdef MULTITHREAD_ON
    void *handle_connection_thread(void *thread_args) {
        client_handle *connection = (client_handle*)thread_args;
        handle_connection(connection->socket, connection->default_route, connection->folder_to_serve);
        close_socket(connection->socket);
        free(connection);
        pthread_exit(NULL);
        return NULL;
    }
#endif