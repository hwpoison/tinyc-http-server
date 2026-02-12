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
    /* ======= Args parse ==================  */
    /* =====================================  */

    if(get_arg_value(argc, argv, "--help") != NULL){
        printf(
            "::: TinyC lightweight http file server (by hwpoison) :::\n"
            "\nBasic usage: %s --port 8081 --folder /my_web\n"
            " example: %s --port 3543 --folder simple_web/index.html\n"
            "\nOptions:\n"
            "\t--ip <ip addr>: Set server IP. Default: ANY (Local/Network).\n"
            "\t--port <port_number>: Port number. Default is %d\n"
            "\t--backlog <number>: Max server listener.\n"
            "\t--max-threads <number>: Max server threads.\n"
            "\t--default-redirect <file_path>: Redirect from / to a specified route. eg: simple_web/index.html\n"
            "\t--folder <folder_path>: Just serve from a specified folder content. eg: simple_web/ \n"
            "\t--no-logs : Disable logging.\n"
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

    if((input_arg = get_arg_value(argc, argv, "--ip")) != NULL) {
        strncpy(server_ip, input_arg, sizeof(server_ip) - 1);
        server_ip[sizeof(server_ip) - 1] = '\0';
    }

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
        write_log(NULL, "Multithreading enabled.");
    #endif

    /* =============================================================  */
    /*        Server socket initialization and configuration          */
    /* =============================================================  */
    write_log(NULL, "Initializing server.");
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

	// Welcome message
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
    /*       Accept connections loop          */
    /* =====================================  */
    // At this point, the server is running and waiting for upcoming connections in loop
    for(;;) {
        #ifdef MULTITHREAD_ON
            // Wait if too many active connections
            pthread_mutex_lock(&thread_mutex);
            while (active_threads >= max_threads) {
                pthread_cond_wait(&thread_cond, &thread_mutex);
            }
            active_threads++;
            pthread_mutex_unlock(&thread_mutex);
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
            // Handle the connection in a thread appart
            pthread_t thread;
            int new_thread = pthread_create(&thread, NULL, handle_connection_thread, (void*)client_conn);
            if(new_thread != 0){
                write_log("error", "pthread_create failed: '%s'", strerror(new_thread));
            }
            pthread_detach(thread);
        #else
            // Handle the connection in the current process 
            handle_connection(client_conn);
        #endif
    }   

    // Close server socket and release memory
    close_socket(server_socket);
    #ifdef MULTITHREAD_ON
        pthread_mutex_destroy(&thread_mutex);
        pthread_cond_destroy(&thread_cond);
    #endif
    #ifdef _WIN32
        WSACleanup();
    #endif

    atexit(close_log_file);
    return 0;
}

char *get_arg_value(int argc, char **argv, char *target_arg){
    for(int arg_idx = 0; arg_idx < argc; arg_idx++){
        if(!strcmp(argv[arg_idx], target_arg)) { // <arg> <value>
            if (arg_idx + 1 < argc)
                return argv[arg_idx+1];
            return "";
        }
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

void send_response(SocketType to_socket, const char *response_content) {
    size_t len = strlen(response_content);  
    write_log(NULL, "Sending %zu bytes.", len);
    if(send(to_socket, response_content, len, 0) < 0){
        write_log(NULL, "Error to sending.\n");
    }
}

void send_file_content(SocketType to_socket, FILE *file){
    char buffer[BUFFER_SIZE] = {0};
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if(send(to_socket, buffer, bytesRead, SEND_D_FLAG ) < 0)
            break;
    }
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

void send_partial_content(SocketType  socket, FILE *file, const char *content_type, size_t file_size, size_t start, size_t end) {
    // Seek the file to the specified range before send
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

void remove_slash_from_start(char* str) {
    size_t length = strlen(str);
    if (length > 0 && str[0] == '/') {
        memmove(str, str + 1, length);
        str[length - 1] = '\0';
    }
}

#ifndef __linux__
WCHAR *utf8_to_wide(const char *s) {
	if (!s) return NULL;

	int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
	if (!n) return NULL;

	WCHAR *w = malloc(n * sizeof(WCHAR));
	if (!w) return NULL;

	if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n)) {
		free(w);
		return NULL;
	}
	return w;
}
#endif

int starts_with(const char *str, const char *word) {
    size_t word_len = strlen(word);
    return strncmp(str, word, word_len) == 0?TRUE:FALSE;
}

void concat_str(char **main_str, const char *to_add) {
    size_t len_add = strlen(to_add);

    if (*main_str == NULL) {
        size_t initial_size = len_add + 1 + HTML_EL_SIZE;
        *main_str = safe_malloc(initial_size);
        (*main_str)[0] = '\0';
    } else {
        size_t len_base = strlen(*main_str);
        size_t needed   = len_base + len_add + 1;

        size_t estimated_capacity = len_base + HTML_EL_SIZE;
        
        if (needed > estimated_capacity) {
            size_t new_capacity = (estimated_capacity * 3) / 2;
            if (new_capacity < needed) new_capacity = needed;
            
            char *temp = realloc(*main_str, new_capacity);
            if (!temp) {
                write_log("error", "Failed to reallocate memory in concat_str");
                return;
            }
            *main_str = temp;
        }
    }
    strcat(*main_str, to_add);
}

void *safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr == NULL) {
        write_log("error", "Error allocating memory.");
        exit(-1);
    }
    return ptr;
}

char *cstrdup(char *string){
   if (string == NULL) {
       write_log("error", "cstrdup called with NULL string");
       return NULL;
   }
   
   size_t string_len = strlen(string) + 1;
   char *dup = safe_malloc(string_len);
   memcpy(dup, string, string_len);
   return dup;
}

FILE *open_a_file(char *file_path){
#ifdef __linux__
        FILE *file = fopen(file_path, "rb");
#else
		WCHAR *w_file_path = utf8_to_wide(file_path);
		if(!w_file_path){
			write_log("error", "Failed to convert path to wide char: '%s'", file_path);
			return NULL;
		}
		FILE *file = _wfopen(w_file_path, L"rb");
		free(w_file_path);
#endif
	return file;
}

fs_type get_fs_type(const char *path) {
#ifdef __linux__
    struct stat st;

    if (stat(path, &st) != 0)
        return FS_NOT_FOUND;

    if (S_ISREG(st.st_mode))
        return FS_FILE;

    if (S_ISDIR(st.st_mode))
        return FS_DIR;

    return FS_NOT_FOUND;

#else
    WCHAR *wpath = utf8_to_wide(path);
    if (!wpath)
        return FS_NOT_FOUND;

    DWORD attrs = GetFileAttributesW(wpath);
    free(wpath);

    if (attrs == INVALID_FILE_ATTRIBUTES)
        return FS_NOT_FOUND;

    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        return FS_DIR;

    return FS_FILE;
#endif
}

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
            write_log("info", "Maximum of showed files exceeded.");
            closedir(dir);
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
	WCHAR *wpath = utf8_to_wide(path);
	WIN32_FIND_DATAW find_data;
	HANDLE h_find;

	if (!wpath) {
		write_log("error", "utf8_to_wide failed for path %s", path);
		free(dir_content);
		return NULL;
	}

	h_find = FindFirstFileW(wpath, &find_data);
	free(wpath);

	if (h_find == INVALID_HANDLE_VALUE) {
		write_log("error", "windows: not possible get directory content %s", path);
		free(dir_content);
		return NULL;
	}

	do {
		char entry_name[EXPLORER_MAX_FILENAME_LENGTH * 3];

		WideCharToMultiByte(
			CP_UTF8, 0,
			find_data.cFileName, -1,
			entry_name, sizeof(entry_name),
			NULL, NULL
		);

		if (!strcmp(entry_name, ".") || !strcmp(entry_name, ".."))
			continue;

		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			strcat(entry_name, "/");

		dir_content[file_n] = safe_malloc(strlen(entry_name) + 1);
		strcpy(dir_content[file_n], entry_name);
		file_n++;

	} while (FindNextFileW(h_find, &find_data));

	FindClose(h_find);
#endif
  *file_amount = file_n;
  dir_content[file_n+1] = NULL;
  
  return dir_content;
}

int url_to_ospath(const char *url_path, char *os_path, size_t max_length){
	int status;
	#ifdef __linux__
		status = snprintf(os_path, max_length, "./%s", url_path);
	#else
		status = snprintf(os_path, max_length, "./%s/*", url_path);
	#endif
	if(status<0||(size_t)status>=max_length){
		return -1;
	}
	return 0;
}

char* os_dir_to_html(const char* path){
	char dir_path[MAX_PATH_LENGTH] = {0};
	url_to_ospath(path, dir_path, MAX_PATH_LENGTH);

	// get dir content
	size_t file_amount;
	char **dir_content = get_dir_content(dir_path, &file_amount);
	if(dir_content==NULL){
		write_log("error", "Error getting dir content");
		return NULL;
	}

	// starts with the file explorer view creation
	char *html_view = NULL;
	char list_element[EXPLORER_MAX_FILENAME_LENGTH];
	
	// header
	sprintf(list_element, FILE_EXPLORER_HEADER, path, file_amount);
	concat_str(&html_view, list_element);

	// special entries
	sprintf(list_element, FILE_EXPLORER_LIST_ELEMENT, "..", "..");
	concat_str(&html_view, list_element);

	// file list
	for(int x=0; x < file_amount; x++){
		sprintf(list_element, FILE_EXPLORER_LIST_ELEMENT, dir_content[x], dir_content[x]);
		concat_str(&html_view, list_element);
	}
	
	// footer
	concat_str(&html_view, FILE_EXPLORER_FOOTER);
	
	for (int i = 0; i < file_amount; i++)
		free(dir_content[i]);
	free(dir_content); 

	return html_view;
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

#ifdef __linux__
    int get_file_size(FILE* file, uint64_t *size_out){
	   if (!file || !size_out)
			return -1;

		int fd = fileno(file);
		if (fd == -1)
			return -1;

		struct stat st;
		if (fstat(fd, &st) != 0)
			return -1;

		*size_out = (uint64_t)st.st_size;

		if (fseeko(file, 0, SEEK_SET) != 0)
			return -1;

		return 0;
    }
#else
	int get_file_size(FILE* file, uint64_t *size_out){
		if (!file || !size_out)
			return -1;

		int fd = _fileno(file);
		if (fd == -1)
			return -1;

		HANDLE hFile = (HANDLE)_get_osfhandle(fd);
		if (hFile == INVALID_HANDLE_VALUE)
			return -1;

		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(hFile, &fileSize))
			return -1;

		*size_out = (uint64_t)fileSize.QuadPart;

		if (_fseeki64(file, 0, SEEK_SET) != 0)
			return -1;

		return 0;
	}
#endif

void socket_error_msg(){
    #ifdef __linux__
        // todo
        perror("Error caused by:");
    #else
        int errCode = WSAGetLastError();
        write_log("error", "Socket error code %d", errCode);
    #endif
}

int read_socket_content(connection_params *conn, char *buffer){
	size_t read_bytes;
#ifdef __linux__
	read_bytes = read(conn->socket, buffer, BUFFER_SIZE);
#else
	read_bytes = recv(conn->socket, buffer, BUFFER_SIZE, 0);
#endif
	if (read_bytes == 0) {
		write_log(NULL, "[%d] Connection closed by client.", conn->socket);
		return -1;
	} else if (read_bytes == -1) {
		socket_error_msg();
		write_log("error", "[%d] Error reading content from client socket.", conn->socket);
		return -1;
	}
	buffer[read_bytes] = '\0';
	return (int)read_bytes;
}

void close_socket(SocketType socket) {
    #ifdef __linux__
        close(socket);
    #else 
        closesocket(socket);
    #endif
    write_log("info", "[%d] Socket closed.", socket);
}

int is_directory_request(const char *url_path){
	int path_len = strlen(url_path);
	if((path_len > 0 && url_path[path_len - 1] == '/') || strcmp(url_path, "") == 0){
		return 1;
	}
	return -1;
}

void handle_connection(connection_params *conn){
	char url_path[MAX_PATH_LENGTH];
	char buffer[BUFFER_SIZE];
	size_t file_size, start_offset, end_offset;

	/* ======================================= */
	/*  Read<->Send loop between client-server  */
	/* ======================================= */
	// At this point, a connection with a client is established and the socket is ready to receive and send requests.
	for(;;){
		int read = read_socket_content(conn, buffer);
		if(read <= 0)
			break;

		// Extrat URI and validate length
		if(extract_URI_from_header(buffer, url_path, sizeof(url_path))){
			send_414_response(conn->socket);
			break;
		}
		decode_url(url_path);
		write_log(NULL, "Handling route: %s", url_path);

		remove_slash_from_start(url_path);
        // Check if the path match with default folder and just serve his content
        if(conn->folder_to_serve != NULL){
            if(!starts_with(url_path, conn->folder_to_serve)){
				send_404_response(conn->socket);	
				break;
			}
        }

		fs_type entrie = get_fs_type(url_path);
		if(strcmp(url_path, "") == 0){
			entrie = FS_DIR;
			if(conn->default_route != NULL){
				write_log("info", "Redirecting to %s", conn->default_route);
				send_302_response(conn->socket, conn->default_route);
			}
		}
		switch(entrie){
			case FS_FILE:
				write_log(NULL, "Finding for '%s' file..", url_path);
				FILE *file = open_a_file(url_path);
				if (file == NULL){
					send_404_response(conn->socket);
					break;
				}
				
				uint64_t file_size;
				if(get_file_size(file, &file_size)){
					write_log("error", "Failed to get file size of '%s'", url_path);
					send_500_response(conn->socket);
					break;
				}

				// Ir the request contains range, send it in chunks, if not, send the entire file
				char* range_header = strstr(buffer, "Range: bytes=");
				const char *mimetype = get_filename_mimetype(url_path);
				if (range_header != NULL) {
					start_offset = 0, end_offset = (file_size > 0) ? file_size -1 : 0;
					sscanf(range_header, "Range: bytes="SIZE_T_FORMAT"-"SIZE_T_FORMAT"", &start_offset, &end_offset);
					write_log(NULL, "Range detected: from "SIZE_T_FORMAT" to " SIZE_T_FORMAT, start_offset, end_offset);
					send_partial_content(
							conn->socket,
							file, 
							mimetype,
							file_size,
							start_offset, 
							end_offset);
				}else{ 
					send_content(
							conn->socket,
							file,
							mimetype,
							file_size);
				}
				fclose(file);
				break;
			case FS_DIR:	
				if(conn->show_explorer){
					write_log(NULL, "[%d] Explorer opened for '%s'", conn->socket, url_path);
					char *html_file_list = os_dir_to_html(url_path);
					if(html_file_list == NULL){
						write_log(NULL, "[%d] Failed to render html file list view for '%s'", conn->socket, url_path);
						send_500_response(conn->socket);
						break;
					}else{
						send_response(conn->socket, html_file_list);
						free((char*)html_file_list);
					}
					break;
				}
			default:
				write_log("error", "The file '%s' could not be opened/found.", url_path);
				send_404_response(conn->socket);
				break;
		}
		break;
	}
	close_socket(conn->socket);
	free(conn);
}

#ifdef MULTITHREAD_ON
void *handle_connection_thread(void *conn) {
	connection_params *connection = (connection_params*)conn;
	handle_connection(connection);
	
    // When connection is closed, free a thread slot
	pthread_mutex_lock(&thread_mutex);
	active_threads--;
	pthread_cond_signal(&thread_cond);
	pthread_mutex_unlock(&thread_mutex);
	
	pthread_exit(NULL);
	return NULL;
}
#endif

