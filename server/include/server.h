#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "client_handler.h"
#include "logging.h"

int setup_server_socket(int port);
void start_server(int server_fd);

#endif // SERVER_H
