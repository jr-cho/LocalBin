#include "../include/server.h"

/*
 * ==============================================================
 *  Function: setup_server_socket
 *  Purpose : Create, bind, and listen on a TCP socket
 * ==============================================================
 */
int setup_server_socket(int port) {
    int server_fd;
    struct sockaddr_in server_addr;

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("[ERROR] Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Setup socket options
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configure address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[ERROR] Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("[ERROR] Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[INFO] Server listening on port %d...\n", port);
    log_event("SERVER", "Listening started");

    return server_fd;
}

/*
 * ==============================================================
 *  Function: start_server
 *  Purpose : Accept incoming connections and spawn threads
 * ==============================================================
 */
void start_server(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_size = sizeof(client_addr);
    pthread_t thread_id;

    while (1) {
        int* new_sock = malloc(sizeof(int));
        if (!new_sock) continue;

        *new_sock = accept(server_fd, (struct sockaddr*)&client_addr, &client_size);
        if (*new_sock < 0) {
            perror("[ERROR] Accept failed");
            free(new_sock);
            continue;
        }

        printf("[INFO] Connection accepted from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        log_event("CONNECT", "New client connection accepted");

        if (pthread_create(&thread_id, NULL, handle_client, (void*)new_sock) != 0) {
            perror("[ERROR] Thread creation failed");
            close(*new_sock);
            free(new_sock);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(server_fd);
}

