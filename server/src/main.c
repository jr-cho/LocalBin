#include "../include/server.h"

/*
 * ==============================================================
 *  Function: main
 *  Purpose : Entry point for LocalBin server
 * ==============================================================
 */
int main(void) {
    int server_fd = setup_server_socket(PORT);
    start_server(server_fd);
    return 0;
}

