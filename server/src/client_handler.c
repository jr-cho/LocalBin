#include "../include/client_handler.h"

/*
 * ==============================================================
 *  Function: handle_client
 *  Purpose : Manage communication between server and one client
 *  Author  : Joshua Gottus
 *  Notes   : Each client runs on its own thread.
 * ==============================================================
 */
void* handle_client(void* socket_desc) {
    int sock = *(int*)socket_desc;
    free(socket_desc);  // Free memory allocated by pthread_create

    char buffer[BUFFER_SIZE];
    int read_size;

    printf("[INFO] Client connected (Socket FD: %d)\n", sock);
    log_event("CONNECT", "Client connected");

    // ==============================================================
    //  Main Communication Loop
    // ==============================================================
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        // Receive data from client
        read_size = recv(sock, buffer, BUFFER_SIZE - 1, 0);

        if (read_size > 0) {
            buffer[read_size] = '\0'; 

            printf("[CLIENT %d]: %s\n", sock, buffer);
            log_event("RECEIVE", buffer);

            // Echo the message back to client
            if (send(sock, buffer, read_size, 0) < 0) {
                perror("[ERROR] Send failed");
                log_event("ERROR", "Failed to send data to client");
                break;
            }

            // Graceful disconnect request
            if (strncmp(buffer, "exit", 4) == 0) {
                printf("[INFO] Client %d requested disconnect.\n", sock);
                log_event("DISCONNECT", "Client requested EXIT");
                break;
            }
        }
        else if (read_size == 0) {
            // Client disconnected normally
            printf("[INFO] Client %d disconnected.\n", sock);
            log_event("DISCONNECT", "Client disconnected");
            break;
        }
        else {
            // recv() failed
            perror("[ERROR] Recv failed");
            log_event("ERROR", "Failed to receive data from client");
            break;
        }
    }

    // ==============================================================
    //  Cleanup
    // ==============================================================
    close(sock);
    printf("[INFO] Connection with client %d closed.\n", sock);
    log_event("INFO", "Client connection closed");

    pthread_exit(NULL);
    return NULL;
}

