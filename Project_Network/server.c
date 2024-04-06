#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define DEFAULT_FILE "index.html"

void *handle_connection(void *socket_desc) {
    int client_socket = *((int *)socket_desc);
    char buffer[BUFFER_SIZE] = {0};
    char response_header[BUFFER_SIZE] = {0};
    char *file_extension;
    FILE *file;

    // Read the HTTP request from the client
    read(client_socket, buffer, BUFFER_SIZE);
    printf("Request received from client:\n%s\n", buffer);

    // Parse HTTP request to get the requested file path
    strtok(buffer, " ");
    char *file_path = strtok(NULL, " ");

    // If no file path provided, serve default file
    if (file_path == NULL || strcmp(file_path, "/") == 0) {
        file_path = DEFAULT_FILE;
    } else {
        // Skip leading '/'
        file_path++;
    }

    // Determine file extension
    file_extension = strrchr(file_path, '.');
    if (file_extension == NULL) {
        printf("Invalid file extension\n");
        close(client_socket);
        free(socket_desc);
        pthread_exit(NULL);
    }

    // Determine Content-Type based on file extension
    char *content_type;
    if (strcmp(file_extension, ".html") == 0) {
        content_type = "text/html";
    } else if (strcmp(file_extension, ".gif") == 0) {
        content_type = "image/gif";
    } else if (strcmp(file_extension, ".jpeg") == 0 || strcmp(file_extension, ".jpg") == 0) {
        content_type = "image/jpeg";
    } else if (strcmp(file_extension, ".mp3") == 0) {
        content_type = "audio/mpeg";
    } else if (strcmp(file_extension, ".pdf") == 0) {
        content_type = "application/pdf";
    } else {
        printf("Unsupported file type\n");
        close(client_socket);
        free(socket_desc);
        pthread_exit(NULL);
    }

    // Prepare HTTP response header
    snprintf(response_header, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "\r\n", content_type);

    // Send the HTTP response header to the client
    write(client_socket, response_header, strlen(response_header));

    // Open and send the requested file to the client
    file = fopen(file_path, "rb");
    if (file == NULL) {
        printf("Error opening file\n");
        close(client_socket);
        free(socket_desc);
        pthread_exit(NULL);
    }

    char file_buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(file_buffer, 1, BUFFER_SIZE, file)) > 0) {
        write(client_socket, file_buffer, bytes_read);
    }

    fclose(file);
    close(client_socket);
    free(socket_desc);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t thread_id;

    // Check if port is specified in command line arguments
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]); // Convert port from string to integer

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Prepare server address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port); // Use specified port

    // Bind socket to address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    // Accept incoming connections and handle them in separate threads
    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
            perror("Accept failed");
            continue;
        }

        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Create thread to handle connection
        int *new_client = malloc(sizeof(int));
        *new_client = client_socket;
        if (pthread_create(&thread_id, NULL, handle_connection, (void *)new_client) < 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
        pthread_detach(thread_id); // Detach the thread
    }

    close(server_socket);

    return 0;
}

