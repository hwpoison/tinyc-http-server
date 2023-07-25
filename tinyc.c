#include "tinyc.h"

int main(int argc, char *argv[]) {
    char *input_arg = NULL;
    char *folder_to_serve = NULL, *default_route = NULL;
    char server_ip[] = "0.0.0.0";
    char client_ip[8] = ":";

    int16_t port = DEFAULT_PORT;
    int16_t backlog = SERVER_BACKLOG;
    int16_t max_threads = MAX_THREADS;
    int8_t show_explorer = TRUE;

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
            "::: TinyC lightweight http server (by hwpoison) :::\n"
            "\nBasic usage: %s --port 8081 --folder /my_web\n"
            " example: %s --port 3543 --folder simple_web/index.html\n"
            "\nOptions:\n"
            "\t--folder <folder_path>: Folder to serve. By default serve all executable location dir content.\n"
            "\t--ip: Set server IP. Default: ANY (Local/Network).\n"
            "\t--port <port_number>: Port number. Default is %d\n"
            "\t--backlog <number>: Max server listener.\n"
            "\t--max-threads <number>: Max server threads.\n"
            "\t--default-redirect <file_path>/: redirect / to default file route. ex: simple_web/index.html\n"
            "\t--no-print : No print log (less mem consumption).\n"
            "\t--no-file-explorer: Disable file explorer.\n"
            ,argv[0], argv[0], DEFAULT_PORT);
        return 0;
    }

    // Get args
    if((input_arg = get_arg_value(argc, argv, "--port")) != NULL)
        port = atoi(input_arg);

    if((input_arg = get_arg_value(argc, argv, "--backlog")) != NULL)
        backlog = atoi(input_arg);

    if((input_arg = get_arg_value(argc, argv, "--max-threads")) != NULL)
        max_threads = atoi(input_arg);

    if((input_arg = get_arg_value(argc, argv, "--ip")) != NULL)
        strcpy(server_ip, input_arg);

    if((get_arg_value(argc, argv, "--no-print")) != NULL)
        print_output = 0;

    if(get_arg_value(argc, argv, "--no-file-explorer") != NULL)
        show_explorer = FALSE;

    if((input_arg = get_arg_value(argc, argv, "--folder")) != NULL)
        folder_to_serve = input_arg;

    default_route = get_arg_value(argc, argv, "--default-redirect");

    print_tinyc_welcome_logo();

    set_shell_text_color("36"); // lightblue
    print(NULL, "Max threads: %d", max_threads);
    print(NULL, "Backlog: %d", backlog);

    #ifdef MULTITHREAD_ON
        pthread_t *active_threads = safe_malloc(sizeof(active_threads)*max_threads);
        int16_t thread_count = 0;
        print(NULL, "Multithreading enabled.");
    #endif

    /* =============================================================  */
    /* ======= Server scket initialization and configuration =======  */
    /* =============================================================  */

    print(NULL, "Initializing server socket.");
    // Winsock init
    #ifdef _WIN32
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            perror("Error with winsock");
            exit(EXIT_FAILURE);
        }
    #endif

    // Create server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error to create server socket.");
        exit(EXIT_FAILURE);
    }

    // Set up the socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(server_ip);
    address.sin_port = htons(port);

    #ifdef __linux__
        struct timeval timeout = { .tv_sec = CLIENT_TIMEOUT, .tv_usec = 0};
    #else
        int timeout = 1000*CLIENT_TIMEOUT; // ms to sec for win
    #endif

    // Bind addr and port
    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        print(NULL, "[x] Error binding the socket to address and port %d.", port);
        perror("Caused by ");
        exit(EXIT_FAILURE);
    }

    // Start to listen incoming connections
    if (listen(server_socket, backlog) < 0) {
        perror("Error to listen connections.");
        exit(EXIT_FAILURE);
    }
    print(NULL, "> Running server at: %s:%d", server_ip, port);
    set_shell_text_color("0");

    /* =====================================  */
    /* ======= Accept connections loop =====  */
    /* =====================================  */
    for(;;) {
        #ifdef MULTITHREAD_ON
            // Check threads limits
            if(thread_count>=max_threads){
                print("error", "Server too busy...\n");
                for(int i = 0; i < max_threads; i++){
                    pthread_join(active_threads[i], NULL);
                }
                thread_count = 0;
                print("info", "All threads free up.\n");
            }
        #endif

        // Accept client new connection
        #ifdef __linux__
            if ((client_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        #else
            if ((client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen)) == INVALID_SOCKET) {
        #endif
                print("error", "Error accepting the connection");
                continue;
        }

        // Get client ip address
        #ifdef __linux__
            inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
        #else
            strcpy(client_ip, inet_ntoa(address.sin_addr));
        #endif

        // Set timeout in send and receive data from client_socket
        if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) == -1 ||
            setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout)) == -1) {
            perror("Error to setup socket timeout.");
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        // Create new thread for handle connection
        print("info", "[%d] Incoming connection from %s",client_socket, client_ip);

        connection_params *params = safe_malloc(sizeof(connection_params));
        params->socket = client_socket;
        params->default_route = default_route;
        params->folder_to_serve = folder_to_serve;
        params->show_explorer = show_explorer;

        #ifdef MULTITHREAD_ON
            pthread_create(&active_threads[thread_count], NULL, handle_connection_thread, (void*)params);
            pthread_detach(active_threads[thread_count]);
            thread_count++;
        #else
            handle_connection(params);
        #endif
    }   

    // Close server socket and release memory
    close_socket(server_socket);
    #ifdef _WIN32
        WSACleanup();
    #endif
    return 0;
}

char *get_arg_value(int argc, char **argv, char *target_arg){
    for(int arg_idx = 0; arg_idx < argc; arg_idx++){
        if(!strcmp(argv[arg_idx], target_arg)) // <arg> <value> 
            return argv[arg_idx+1]==NULL?"":argv[arg_idx+1];
    }
    return NULL;
}

inline void print_tinyc_welcome_logo(){
    set_shell_text_color("32");
    print(NULL, "####  Welcome to tinyC!  ####");
    set_shell_text_color("0");
}

void set_shell_text_color(const char* color) {
    printf("\033[%sm", color);
}

void print(const char* type, const char* msg, ...) {
    if (print_output == TRUE) {
        va_list args;
        char* full_date = get_current_datetime();
        va_start(args, msg);
        if (type != NULL) {
            if(!strcmp(type, "error")){
                set_shell_text_color("31");
                printf("[ERROR][%s] ", full_date);
            }
            if(!strcmp(type, "info")){
                set_shell_text_color("35"); // purple
                printf("[INFO][%s] ", full_date);
            }
            set_shell_text_color("0");
        } else {
            printf("[%s] ", full_date);
        }

        vprintf(msg, args);
        printf("\n");
        va_end(args);
        // free(full_date);
    }
}

void send_206_http_response(SocketType  socket, FILE *file, const char *content_type, size_t file_size, size_t start, size_t end) {
    // Seek the file to the specified rangue before send
    fseek(file, start, SEEK_SET);
    // Check if the requested range is within the file size
    if (start > file_size || end > file_size) {
        print("error", "[!] Error: requested range is out of bounds.");
        send_500_response(socket);
        return;
    }

    // Send header with range and content length (for video html stream content)
    char header[MAX_HEADER_SIZE];
    snprintf(header, MAX_HEADER_SIZE, "HTTP/1.1 206 Partial Content\r\n"
                    "Connection: keep-alive\r\n"
                    "Accept-Ranges: bytes\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Range: bytes " SIZE_T_FORMAT "-" SIZE_T_FORMAT "/" SIZE_T_FORMAT "\r\n"
                    "Content-Length: " SIZE_T_FORMAT "\r\n\r\n", content_type, start, end, file_size, end - start + 1);
    send_response(socket, header);
    send_file_in_chuncks(socket, file);
    print("info", "Response 206 done.");
}

void send_200_http_response(SocketType  socket, FILE *file, const char *content_type, size_t content_length) {
    char header[MAX_HEADER_SIZE];
    snprintf(header, MAX_HEADER_SIZE, "HTTP/1.1 200 OK\r\n"
                    "Connection: keep-alive\r\n"
                    "Accept-Ranges: bytes\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: " SIZE_T_FORMAT "\r\n\r\n", content_type, content_length);
    send_response(socket, header);
    send_file_in_chuncks(socket, file);
    print("info", "Response 200 Done.");
}

void send_302_response(SocketType  socket, char *uri) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, MAX_HEADER_SIZE, HTTP_302_REDIRECTION, uri);
    send_response(socket, buffer);
    print("info", "302 redirection to %s", uri);
}

void send_404_response(SocketType  socket) {
    send_response(socket, HTTP_404_NOT_FOUND);
    print("info", "404 not found.");
}

void send_500_response(SocketType  socket) {
    send_response(socket, HTTP_500_INTERNAL_ERROR);
    print("error", "500 server side error.");
}

int starts_with(const char *str, const char *word) {
    size_t word_len = strlen(word);
    return strncmp(str, word, word_len) == 0?TRUE:FALSE;
}

#ifdef __linux__
    size_t get_file_length(const char* filename){
        FILE *file = fopen(filename, "rb");
        if (file == NULL) {
            print("error", "Error to open the file for get file size : %s", filename);
            return 0;
        }
        fseek(file, 0, SEEK_END);
        size_t fileLength = ftell(file);
        fclose(file);
        return fileLength;
    }
#else
    size_t get_file_length(const char* filename) {
        HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            print("error", "Error to open the file for get file size : %s", filename);
            return 0;
        }
        size_t fileSize = GetFileSize(hFile, NULL);
        CloseHandle(hFile);
        return fileSize;
    }
#endif

void send_response(SocketType to_socket, const char *response_content) {
    print(NULL, "Sending %d bytes.", strlen(response_content));
    if(send(to_socket, response_content, strlen(response_content), 0) < 0){
        print(NULL, "Error to sending.\n");
    }
}

void send_file_in_chuncks(SocketType to_socket, FILE *file){
    char buffer[BUFFER_SIZE] = {0};
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

char* get_current_datetime() {
    time_t now;
    struct tm current_date;
    static char currenttime[30];
    time(&now);
    #ifdef __linux__
        localtime_r(&now, &current_date);
    #else 
        localtime_s(&current_date, &now);
    #endif
    strftime(currenttime, sizeof(currenttime), "%Y-%m-%d %H:%M:%S", &current_date);
    return currenttime;
}

void close_socket(SocketType socket) {
    #ifdef __linux__
        close(socket);
    #else 
        closesocket(socket);
    #endif
    print("info", "[%d] Socket closed.", socket);
}

char *extract_URI_from_header(char *header_content){
    char *extracted_uri;
    char *start, *end;
    int length;
    if((start = strchr(header_content, ' ')) != NULL &&
        (end = strchr(start+1, ' '))!=NULL){
        length = (end-header_content)-(start-header_content)-1;
        extracted_uri = (char*)malloc(MAX_PATH_LENGTH);
        strncpy(extracted_uri, start+1, length);
        extracted_uri[length] = '\0';
        return extracted_uri;
    }
    return "/";
}

void normalize_path(char* str) {
    // replace %20 for spaces.
    char *p20 = "%20";
    char* pos = strstr(str, p20);
    while (pos != NULL) {
        memmove(pos + 1, pos + 3, strlen(pos + 2));
        *pos = ' ';
        pos = strstr(pos + 1, p20);
    }
}

void *safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr == NULL) {
        print("error", "Error allocating memory.");
        exit(-1);
    }
    return ptr;
}

char **get_list_file(const char* path, size_t *file_amount) {
  char **dir_content = (char**)safe_malloc(EXPLORER_MAX_FILES * sizeof(char*) + 1);
  char entry_name[EXPLORER_MAX_FILENAME_LENGTH];
  int file_n = 0;
  #ifdef __linux__
        DIR *dir;
        struct dirent *entry;
        dir = opendir(path);

        if(dir == NULL){
            print("error", "linux: not possible get directory content %s", path);
            return NULL;
        }

        while ((entry= readdir(dir)) != NULL){
            if(strlen(entry->d_name) > EXPLORER_MAX_FILENAME_LENGTH){
                print("error", "File name too long.");
                continue;
            }
            if(file_n >= EXPLORER_MAX_FILES){
                print("info", "Maximum of showed files exceded.");
                break;
            }
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                continue;

            if(entry->d_type == DT_DIR)
                sprintf(entry_name, "%s/", entry->d_name);
            else
                sprintf(entry_name, "%s", entry->d_name);

            dir_content[file_n] = (char*)safe_malloc(strlen(entry_name) + 1);
            strcpy(dir_content[file_n], entry_name);
            file_n++;
        }
        closedir(dir);
  #else
        WIN32_FIND_DATA find_data;
        HANDLE h_find;
        h_find = FindFirstFile(path, &find_data);

        if (h_find == INVALID_HANDLE_VALUE){
            print("error", "windows: not possible get directory content %s", path);
            return NULL;
        }

        while(FindNextFile(h_find, &find_data)){
                if(file_n >= EXPLORER_MAX_FILES){
                    print("info", "Maximum of files exceded.");
                    break; 
                }
                if(strlen(find_data.cFileName) > EXPLORER_MAX_FILENAME_LENGTH){
                    print("error", "File name too long.");
                    continue;
                }
                if (!strcmp(find_data.cFileName, ".") || !strcmp(find_data.cFileName, ".."))
                    continue;

                if(find_data.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY)
                    sprintf(entry_name, "%s/", find_data.cFileName);
                else
                    sprintf(entry_name, "%s", find_data.cFileName);

                dir_content[file_n] = (char*)safe_malloc(strlen(entry_name) + 1);
                strcpy(dir_content[file_n], entry_name);
                file_n++;
        }
      FindClose(h_find);
#endif
  *file_amount = file_n;
  dir_content[file_n+1] = NULL;
  
  return dir_content;
}

void concatenate_string(char** str, const char* new_str) {
    if (*str == NULL) {
        *str = (char*)safe_malloc(strlen(new_str) + 1);
        strcpy(*str, new_str);
    } else {
        size_t currentLength = strlen(*str);
        size_t newLength = strlen(new_str);
        char* temp = (char*)realloc(*str, currentLength + newLength + 1);
        if (temp == NULL) {
            print("error", "Error reallocating memory");
            exit(EXIT_FAILURE);
        }
        *str = temp;
        strcat(*str, new_str);
    }
}

void handle_connection(connection_params *conn){
    char *file_path = NULL;
    char buffer[BUFFER_SIZE] = {0};

    int8_t in_folder = FALSE; // Only serve files into specific folder
    size_t read_bytes, file_size, start_offset, end_offset;

    /* ====================================== */
    /* =Read-Send loop between client-server= */
    /* ====================================== */
    
     for(;;){
        print(NULL, "[%d] Reading from client...", conn->socket);

        #ifdef __linux__
            read_bytes = read(conn->socket, buffer, BUFFER_SIZE);
        #else
            read_bytes = recv(conn->socket, buffer, BUFFER_SIZE, 0);
        #endif

        if (read_bytes == 0) {
            print(NULL, "[%d] Connection closed by client.", conn->socket);
            break;
        } else if (read_bytes == -1) {
            print("error", "[%d] Error reading content from client socket.", conn->socket);
            break;
        }
        
        // Extract URI from client recv header
        file_path = extract_URI_from_header(buffer);

        normalize_path(file_path);
        print(NULL, "Handling route: %s", file_path);

        // Check if uri path == '/' and redirect to default route
        if(strcmp(file_path, "/") == 0 && conn->default_route != NULL){
                print("info", "Redirecting to %s", conn->default_route);
                send_302_response(conn->socket, conn->default_route);
        }
        
        remove_slash_from_start(file_path);
        // Check if the path match with default folder
        if(conn->folder_to_serve != NULL){
            remove_slash_from_start(conn->folder_to_serve); 
            in_folder = starts_with(file_path, conn->folder_to_serve);
        }
        
        /* =====================================  */
        /* =======      File explorer      =====  */
        /* =====================================  */
        if(conn->show_explorer == TRUE &&
        (file_path[strlen(file_path)-1] == '/' || strcmp(file_path, "") == 0)) {
            // If folder to serve is specified and path did not match, send a 404
            if(conn->folder_to_serve!=NULL && !in_folder){
                send_404_response(conn->socket);
                break;
            }
            #ifdef __linux__
                sprintf(buffer, "./%s", file_path);
            #else
                sprintf(buffer, "./%s/*", file_path);
            #endif

            size_t file_amount;
            print(NULL, "[%d] Explorer opened for '%s'", conn->socket, buffer);
            char **dir_content = get_list_file(buffer, &file_amount);
            char *file_html_list = NULL;

            sprintf(buffer, FILE_EXPLORER_HEADER, file_path, file_amount);
            concatenate_string(&file_html_list, buffer);

            sprintf(buffer, "<a href='%s'>%s</a><br>", "..", "..");
            concatenate_string(&file_html_list, buffer);

            // List all files
            char list_element[EXPLORER_MAX_FILENAME_LENGTH];
            for(int x=0; x < file_amount; x++){
                sprintf(list_element, FILE_EXPLORER_LIST_ELEMENT, dir_content[x], dir_content[x]);
                list_element[strlen(list_element)] = '\0';
                concatenate_string(&file_html_list, list_element);
            }

            concatenate_string(&file_html_list, FILE_EXPLORER_FOOTER);
            send_response(conn->socket, file_html_list);

            free(file_html_list);
            for (int i = 0; i < file_amount; i++)
                free(dir_content[i]);
            free(dir_content);
            break;
        }

        // Open the file
        print(NULL, "Finding for %s file..", file_path);
           
        FILE *file = fopen(file_path, "rb");

        // If file is not found send a 404
        if (file == NULL) {
            print("error", "The file '%s' could not be opened/found.", file_path);
            send_404_response(conn->socket);
            break;

        // Serve the file
        } else {
            file_size = get_file_length(file_path);
            start_offset = 0, end_offset = file_size -1;
            print(NULL, "File size: "SIZE_T_FORMAT, file_size);

            // Check if the request is has a "Range" header and extract range to stream
            char* range_header = strstr(buffer, "Range: bytes=");
            if (range_header != NULL) {
                sscanf(range_header, "Range: bytes="SIZE_T_FORMAT"-"SIZE_T_FORMAT"", &start_offset, &end_offset);
                print(NULL, "Range detected: from "SIZE_T_FORMAT" to " SIZE_T_FORMAT, start_offset, end_offset);
                send_206_http_response(
                    conn->socket,
                    file, 
                    get_filename_mimetype(file_path), 
                    file_size,
                    start_offset, 
                    end_offset);
            }else{ 
                send_200_http_response(
                    conn->socket,
                    file,
                    get_filename_mimetype(file_path),
                    file_size);
            }
            fclose(file);
        }
    }
    free(file_path);
    close_socket(conn->socket);
}

#ifdef MULTITHREAD_ON
    void *handle_connection_thread(void *thread_args) {
        connection_params *connection = (connection_params*)thread_args;
        handle_connection(connection);
        free(connection);
        pthread_exit(NULL);
        return NULL;
    }
#endif