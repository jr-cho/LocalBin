#include "server.h"
#include "../common/common.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void handle_signal(int sig) {
    printf("\n[INFO] Caught signal %d, closing server...\n", sig);
    server_running = 0;
}

int main(int argc, char *argv[]) {
    int port = 8080;
    if (argc > 1)
        port = atoi(argv[1]);

    printf("[INFO] Starting LocalBin server on port %d...\n", port);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    init_logging();

    printf("[INFO] Waiting for client connections...\n");
    start_server(port);

    log_message("INFO", "Server shutting down...");
    cleanup_user_data();
    log_message("INFO", "Cleanup complete. Goodbye.");

}

