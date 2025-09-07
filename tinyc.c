#include "tinyc.h"

int main(int argc, char *argv[]) {
    char *input_arg = NULL;
    char *folder_to_serve = NULL, *default_route = NULL;
    char server_ip[255] = "0.0.0.0";
    #ifndef __linux__
        char client_ip[8] = ":";
    #else
        char client_ip[INET_ADDRSTRLEN] = ":";
    #endif

    int16_t port = DEFAULT_PORT;
    int16_t backlog = SERVER_BACKLOG;
    int16_t max_threads = MAX_THREADS;
    int8_t show_explorer = TRUE;

    #ifndef __linux__
        setlocale(LC_ALL, "");
    #endif
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
            "\t--folder <folder_path>: Folder to serve. By default is a relative path due to executable location.\n"
            "\t--ip: Set server IP. Default: ANY (Local/Network).\n"
            "\t--port <port_number>: Port number. Default is %d\n"
            "\t--backlog <number>: Max server listener.\n"
            "\t--max-threads <number>: Max server threads.\n"
            "\t--default-redirect <file_path>/: redirect / to default file route. ex: simple_web/index.html\n"
            "\t--no-logs : No print log (Less I/O bound due to stdout and less memory consumption)).\n"
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

    if((get_arg_value(argc, argv, "--no-logs")) != NULL)
        no_logs = TRUE;

    if(get_arg_value(argc, argv, "--no-file-explorer") != NULL)
        show_explorer = FALSE;

    if((input_arg = get_arg_value(argc, argv, "--folder")) != NULL)
        folder_to_serve = input_arg;

    default_route = get_arg_value(argc, argv, "--default-redirect");

    set_shell_text_color("36"); // lightblue
    write_log(NULL, "Max threads: %d", max_threads);
    write_log(NULL, "Backlog: %d", backlog);

    #ifdef MULTITHREAD_ON
        pthread_t *all_threads = safe_malloc(sizeof(pthread_t)*max_threads);
        int16_t thread_count = 0;
        write_log(NULL, "Multithreading enabled.");
    #endif

    /* =============================================================  */
    /* ======= Server scket initialization and configuration =======  */
    /* =============================================================  */
    write_log(NULL, "Initializing server socket.");
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
        write_log(NULL, "[x] Error binding the socket to address and port %s:%d.", server_ip, port);
        socket_error_msg();
        exit(EXIT_FAILURE);
    }

    // Start to listen incoming connections
    if (listen(server_socket, backlog) < 0) {
        perror("Error to listen connections.");
        exit(EXIT_FAILURE);
    }
    if(!no_logs){
        set_shell_text_color("32");
        printf("####  Welcome to tinyC! #### (%s)\n", __TIMESTAMP__);
        printf("#### Running at %s:%d\n", server_ip, port);
        set_shell_text_color("0");
    }

    write_log(NULL, "==> Tinyc server started");
    write_log(NULL, "> Running server at: %s:%d", server_ip, port);
    set_shell_text_color("0");

    /* =====================================  */
    /* ======= Accept connections loop =====  */
    /* =====================================  */
    // At this point, the server is running and waiting for upcoming connections
    for(;;) {
        #ifdef MULTITHREAD_ON
            // Check threads limits, if reach the max, waits until all are finished before 
            // open new one.
            if(thread_count >= max_threads){
                write_log("error", "Server too busy...\n");
                for(int i = 0; i < max_threads - 1; i++){
                    pthread_join(all_threads[i], NULL);
                    all_threads[i] = 0;
                }
                thread_count = 0;
                write_log("info", "All threads free up.\n");
            }
        #endif

        // Accept client new connection
        #ifdef __linux__
            if ((client_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        #else
            if ((client_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen)) == INVALID_SOCKET) {
        #endif
            write_log("error", "Error accepting the connection");
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

        // Prepare to handle the incoming connection
        write_log("info", "[%d] Incoming connection from %s", client_socket, client_ip);

        connection_params *client_conn = safe_malloc(sizeof(connection_params));
        client_conn->socket = client_socket;
        client_conn->default_route = default_route;
        client_conn->folder_to_serve = folder_to_serve;
        client_conn->show_explorer = show_explorer;

        #ifdef MULTITHREAD_ON
            // handle the new connection in a thread apart
            int new_thread = pthread_create(&all_threads[thread_count], NULL, handle_connection_thread, (void*)client_conn);
            if(new_thread != 0){
                write_log("error", "pthread_create failed: '%s'", strerror(new_thread));
            }
            pthread_detach(all_threads[thread_count]);
            thread_count++;
        #else
            // handle the connection in a single thread
            handle_connection(client_conn);
        #endif
    }   

    // Close server socket and release memory
    close_socket(server_socket);
    #ifdef _WIN32
        WSACleanup();
    #endif

    atexit(close_log_file);
    return 0;
}

char *get_arg_value(int argc, char **argv, char *target_arg){
    for(int arg_idx = 0; arg_idx < argc; arg_idx++){
        if(!strcmp(argv[arg_idx], target_arg)) // <arg> <value> 
            return argv[arg_idx+1]==NULL?"":argv[arg_idx+1];
    }
    return NULL;
}

void set_shell_text_color(const char* color) {
    printf("\033[%sm", color);
}

void init_log_file() {
    log_file = fopen(LOG_FILE_NAME, "a");
    if (!log_file) {
        perror("Can't create log file.");
        exit(1);
    }
}

void close_log_file() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

void write_log(const char* type, const char* msg, ...) {
    if(!no_logs){
        if(log_file==NULL)
            init_log_file();
    
        va_list args;
        char* full_date = get_current_datetime();
        if (!log_file) return;

        va_start(args, msg);
        if (type != NULL) {
            if(!strcmp(type, "error")){
                fprintf(log_file, "[ERROR][%s] ", full_date);
            }
            if(!strcmp(type, "info")){
                fprintf(log_file, "[INFO][%s] ", full_date);
            }
            if(!strcmp(type, "debug")){
                printf("[DEBUG][%s]", full_date);
                vprintf(msg, args);
                printf("\n");
                fprintf(log_file, "[DEBUG][%s]", full_date);
            }
        } else {
            fprintf(log_file, "[%s] ", full_date);
        }

        vfprintf(log_file, msg, args);
        fprintf(log_file, "\n");
        va_end(args);
    }
}

void send_response(SocketType to_socket, const char *response_content) {
    write_log(NULL, "Sending %d bytes.", strlen(response_content));
    if(send(to_socket, response_content, strlen(response_content), 0) < 0){
        write_log(NULL, "Error to sending.\n");
    }
}

void send_partial_content(SocketType  socket, FILE *file, const char *content_type, size_t file_size, size_t start, size_t end) {
    // Seek the file to the specified rangue before send
    fseek(file, start, SEEK_SET);
    // Check if the requested range is within the file size
    if (start > file_size || end > file_size) {
        write_log("error", "[!] Error: requested range is out of bounds.");
        send_500_response(socket);
        return;
    }

    // Send header with range and content length (for video html stream content)
    char header[MAX_HEADER_SIZE];
    snprintf(header, MAX_HEADER_SIZE, "HTTP/1.1 206 Partial Content\r\n"
                    "Connection: keep-alive\r\n"
                    "Keep-Alive: timeout=5\r\n"
                    "Accept-Ranges: bytes\r\n"
                    "Content-Type: %s; charset=utf-8\r\n"
                    "Content-Range: bytes " SIZE_T_FORMAT "-" SIZE_T_FORMAT "/" SIZE_T_FORMAT "\r\n"
                    "Content-Length: " SIZE_T_FORMAT "\r\n\r\n", content_type, start, end, file_size, end - start + 1);
    send_response(socket, header);
    send_file_content(socket, file); // then send the file fragment
    write_log("info", "Response 206 done.");
}

void send_content(SocketType socket, FILE *file, const char *content_type, size_t content_length) {
    char header[MAX_HEADER_SIZE];
    snprintf(header, MAX_HEADER_SIZE, "HTTP/1.1 200 OK\r\n"
                    "Connection: keep-alive\r\n"
                    "Keep-Alive: timeout=5\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Accept-Ranges: bytes\r\n"
                    "Content-Type: %s; charset=utf-8\r\n"
                    "Content-Length: " SIZE_T_FORMAT "\r\n\r\n", content_type, content_length);
    send_response(socket, header);
    send_file_content(socket, file); // then the file content
    write_log("info", "Response 200 Done.");
}

void send_302_response(SocketType  socket, char *uri) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, MAX_HEADER_SIZE, HTTP_302_REDIRECTION, uri);
    send_response(socket, buffer);
    write_log("info", "302 redirection to %s", uri);
}

void send_404_response(SocketType  socket) {
    send_response(socket, HTTP_404_NOT_FOUND);
    write_log("info", "404 not found.");
}

void send_200_response(SocketType  socket) {
    send_response(socket, HTTP_200_OK);
    write_log("info", "OK");
}


void send_414_response(SocketType socket){
    send_response(socket, HTTP_414_URL_TOO_LONG);
    write_log("info", "404 not found.");
}

void send_500_response(SocketType  socket) {
    send_response(socket, HTTP_500_INTERNAL_ERROR);
    write_log("error", "500 server side error.");
}

int starts_with(const char *str, const char *word) {
    size_t word_len = strlen(word);
    return strncmp(str, word, word_len) == 0?TRUE:FALSE;
}

#ifdef __linux__
    size_t get_file_length(const char* filename){
        FILE *file = fopen(filename, "rb");
        if (file == NULL) {
            write_log("error", "Error to open the file for get file size : %s", filename);
            return 0;
        }
        fseek(file, 0, SEEK_END);
        size_t fileLength = ftell(file);
        fclose(file);
        return fileLength;
    }
#else
    uint64_t get_file_length(const char* filename) {
        HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            write_log("error", "Error to open the file for get file size : %s", filename);
            return 0;
        }
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(hFile, &fileSize)) {
            CloseHandle(hFile);
            write_log("error", "Error to get file size : %s", filename);
            return 0;
        }
        CloseHandle(hFile);
        return (uint64_t)fileSize.QuadPart;
    }
#endif

char *cstrdup(char *string){
   size_t string_len = strlen(string) + 1;
   char *dup = malloc(string_len);
   if(dup != NULL){
        memcpy(dup, string, string_len);
   } 
   return dup;
}

void send_file_content(SocketType to_socket, FILE *file){
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
    write_log("info", "[%d] Socket closed.", socket);
}


int extract_URI_from_header(char *header_content, char *output_buffer, size_t buffer_size) {
    char *start, *end;
    int length;
    
    // Initialize output buffer with default value
    strncpy(output_buffer, "/", buffer_size - 1);
    output_buffer[buffer_size - 1] = '\0';
    
    if ((start = strchr(header_content, ' ')) != NULL &&
        (end = strchr(start + 1, ' ')) != NULL) {
        
        length = (end - start - 1);
        
        if (length <= 0 || length >= MAX_PATH_LENGTH || length >= (int)buffer_size) {
            write_log("error", "URI too long or invalid.");
            return 1; // output_buffer already contains "/"
        }
        
        strncpy(output_buffer, start + 1, length);
        output_buffer[length] = '\0';
        return 0;
    }
}


void decode_url(char* url) {
    char *url_p = url;
    int decoded_char;
    while (*url_p) {
        if (*url_p == '%' && isxdigit(*(url_p + 1)) && isxdigit(*(url_p + 2))) {
            char hex[3] = {url_p[1], url_p[2], '\0'};
            sscanf(hex, "%x", &decoded_char);
            memmove(url_p + 1, url_p + 3, strlen(url_p + 2) + 1); 
            *url_p = decoded_char;
        }
        url_p++;
    }
}

void *safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr == NULL) {
        write_log("error", "Error allocating memory.");
        exit(-1);
    }
    return ptr;
}

/* Returns a list with all files entries from path and write the file count un file_mount */
char **get_dir_content(const char* path, size_t *file_amount) {
  char **dir_content = (char**)safe_malloc(EXPLORER_MAX_FILES * sizeof(char*) + 1);
  char entry_name[EXPLORER_MAX_FILENAME_LENGTH + 2 ];
  int file_n = 0;

  #ifdef __linux__
    DIR *dir;
    struct dirent *entry;
    dir = opendir(path);
    struct stat file_stat;
    if(dir == NULL){
        write_log("error", "linux: not possible get directory content %s", path);
        free(dir_content);
        return NULL;
    }

    // compose the current dir list
    while ((entry= readdir(dir)) != NULL){
        if(strlen(entry->d_name) > EXPLORER_MAX_FILENAME_LENGTH){
            write_log("error", "File name too long.");
            continue;
        }

        if(file_n >= EXPLORER_MAX_FILES){
            write_log("info", "Maximum of showed files exceded.");
            break;
        }

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        
        snprintf(entry_name, sizeof(entry_name) , "%s/%s", path, entry->d_name);

        if (stat(entry_name, &file_stat) == 0) {
            if (S_ISDIR(file_stat.st_mode))
                sprintf(entry_name, "%s/", entry->d_name);
            else
                sprintf(entry_name, "%s", entry->d_name);
        }

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
        write_log("error", "windows: not possible get directory content %s", path);
        free(dir_content);
        return NULL;
    }

    while(FindNextFile(h_find, &find_data)){
        if(file_n >= EXPLORER_MAX_FILES){
            write_log("info", "Maximum of files exceded.");
            break; 
        }

        if(strlen(find_data.cFileName) > EXPLORER_MAX_FILENAME_LENGTH){
            write_log("error", "File name too long.");
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

void concat_str(char **main_str, const char *to_add) {
    size_t len_add = strlen(to_add);

    if (*main_str == NULL) {
        size_t initial_size = len_add + 1 + HTML_EL_SIZE;
        *main_str = malloc(initial_size);
        if (!*main_str) {
            return;
        }
        (*main_str)[0] = '\0';
    } else {
        size_t len_base = strlen(*main_str);
        size_t needed   = len_base + len_add + 1;

        char *temp = realloc(*main_str, needed + HTML_EL_SIZE);
        if (!temp) {
            return;
        }
        *main_str = temp;
    }

    strcat(*main_str, to_add);
}


void socket_error_msg(){
    #ifdef __linux__
        // todo
        perror("Error caused by:");
    #else
        int errCode = WSAGetLastError();
        write_log("error", "Socket error code %d", errCode);
    #endif
}

void handle_connection(connection_params *conn){
    char file_path[MAX_PATH_LENGTH] = {0};
    char buffer[BUFFER_SIZE] = {0};
    int8_t in_folder = FALSE; // Only serve files into specific folder
    size_t read_bytes, file_size, start_offset, end_offset;

    /* ====================================== */
    /* =Read-Send loop between client-server= */
    /* ====================================== */
    // At this point, a connection with a client is established and the socket is ready to receive and send requests.
    for(;;){
        #ifdef __linux__
            read_bytes = read(conn->socket, buffer, BUFFER_SIZE);
        #else
            read_bytes = recv(conn->socket, buffer, BUFFER_SIZE, 0);
        #endif

        if (read_bytes == 0) {
            write_log(NULL, "[%d] Connection closed by client.", conn->socket);
            break;
        } else if (read_bytes == -1) {
            socket_error_msg();
            write_log("error", "[%d] Error reading content from client socket.", conn->socket);
            break;
        }

        // Read and extract URI from the recv request
        if(extract_URI_from_header(buffer, file_path, sizeof(file_path))){
            send_414_response(conn->socket);
            break;
        }

        decode_url(file_path);

        if(strcmp(file_path, "/test")==0){
            send_200_response(conn->socket);
            break;
        }

        write_log(NULL, "Handling route: %s", file_path);

        // Check if uri path == '/' and redirect to default route
        if(strcmp(file_path, "/") == 0 && conn->default_route != NULL){
            write_log("info", "Redirecting to %s", conn->default_route);
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
        size_t path_len = strlen(file_path);
        char current_path[EXPLORER_MAX_FILENAME_LENGTH] = {0};

		if(conn->show_explorer == TRUE &&
			((path_len > 0 && file_path[path_len - 1] == '/') || strcmp(file_path, "") == 0)){
				// If folder to serve is specified and path did not match, send a 404
				if(conn->folder_to_serve!=NULL && !in_folder){
					send_404_response(conn->socket);
					break;
				}

            // get current path
            #ifdef __linux__
                sprintf(current_path, "./%s", file_path);
            #else
                sprintf(current_path, "./%s/*", file_path);
            #endif

            // get dir content
            size_t file_amount;
            write_log(NULL, "[%d] Explorer opened for '%s'", conn->socket, current_path);
            char **dir_content = get_dir_content(current_path, &file_amount);
            if(dir_content==NULL){
               write_log("error", "Error getting dir content");
               send_500_response(conn->socket);
               break;
            }

            ///////// Create the html view /////////
            char *file_html_list = NULL;
            char list_element[EXPLORER_MAX_FILENAME_LENGTH];
            // Explorer header
            sprintf(list_element, FILE_EXPLORER_HEADER, file_path, file_amount);
            concat_str(&file_html_list, list_element);

            // Special entries
            sprintf(list_element, FILE_EXPLORER_LIST_ELEMENT, "..", "..");
            concat_str(&file_html_list, list_element);

            // List all files
            for(int x=0; x < file_amount; x++){
                sprintf(list_element, FILE_EXPLORER_LIST_ELEMENT, dir_content[x], dir_content[x]);
                list_element[strlen(list_element)] = '\0';
                concat_str(&file_html_list, list_element);
            }
            // The footer
            concat_str(&file_html_list, FILE_EXPLORER_FOOTER);

            // send dir
            send_response(conn->socket, file_html_list);

            free(file_html_list);
            for (int i = 0; i < file_amount; i++)
                free(dir_content[i]);
            free(dir_content); 
            break;
        }

        // Open the file
        write_log(NULL, "Finding for '%s' file..", file_path);
        FILE *file = fopen(file_path, "rb");

        // If file is not found send a 404
        if (file == NULL) {
            write_log("error", "The file '%s' could not be opened/found.", file_path);
            send_404_response(conn->socket);
            
            break;

        // Serve the file
        } else {
            file_size = get_file_length(file_path);
            start_offset = 0, end_offset = file_size -1;
            write_log(NULL, "File size: "SIZE_T_FORMAT, file_size);

            // Check if the request is has a "range" header and extract range to stream
            char* range_header = strstr(buffer, "Range: bytes=");
            if (range_header != NULL) {
                sscanf(range_header, "Range: bytes="SIZE_T_FORMAT"-"SIZE_T_FORMAT"", &start_offset, &end_offset);
                write_log(NULL, "Range detected: from "SIZE_T_FORMAT" to " SIZE_T_FORMAT, start_offset, end_offset);
                send_partial_content(
                    conn->socket,
                    file, 
                    get_filename_mimetype(file_path), 
                    file_size,
                    start_offset, 
                    end_offset);
            }else{ 
                send_content(
                    conn->socket,
                    file,
                    get_filename_mimetype(file_path),
                    file_size);
            }
            
            fclose(file);
        }
    }
    close_socket(conn->socket);
    free(conn);
}

#ifdef MULTITHREAD_ON
    void *handle_connection_thread(void *conn) {
        connection_params *connection = (connection_params*)conn;
        handle_connection(connection);
        pthread_exit(NULL);
        return NULL;
    }
#endif
