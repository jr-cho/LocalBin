#include "server.h"
#include <signal.h>
#include <errno.h>

volatile int server_running = 1;
static int listen_sock = -1;

static void sigint_handler(int sig) {
    UNUSED(sig);
    log_message("INFO", "SIGINT received; shutting down server");
    server_running = 0;
    if (listen_sock != -1) close(listen_sock);
}

int start_server(int port) {
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    init_logging();
    log_message("INFO", "Server initializing");

    signal(SIGINT, sigint_handler);
    server_running = 1;

    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        handle_error("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)port);

    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        handle_error("bind");
        close(listen_sock);
        return -1;
    }

    if (listen(listen_sock, SERVER_BACKLOG) < 0) {
        handle_error("listen");
        close(listen_sock);
        return -1;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Server listening on port %d", port);
    log_message("INFO", buf);
    printf("[SERVER] %s\n", buf);

    while (server_running) {
        int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            if (errno == EINTR) continue;
            log_message("ERROR", "accept failed");
            continue;
        }
        ClientThreadArgs *args = malloc(sizeof(ClientThreadArgs));
        if (!args) {
            log_message("ERROR", "malloc failed for client args");
            close(client_sock);
            continue;
        }
        args->client_sock = client_sock;
        args->client_addr = client_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, args) != 0) {
            log_message("ERROR", "pthread_create failed");
            close(client_sock);
            free(args);
            continue;
        }
        pthread_detach(tid);

        snprintf(buf, sizeof(buf), "Accepted %s:%d",
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        log_message("INFO", buf);
    }

    if (listen_sock != -1) {
        close(listen_sock);
        listen_sock = -1;
    }
    log_message("INFO", "Server stopped");
    return 0;
}
