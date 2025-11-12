#ifndef SERVER_H
#define SERVER_H

#include "../common/common.h"
#include "../common/protocol.h"
#include "client_handler.h"

#define SERVER_BACKLOG 16
extern volatile int server_running;

int start_server(int port);
extern volatile int server_running;

#endif /* SERVER_H */
