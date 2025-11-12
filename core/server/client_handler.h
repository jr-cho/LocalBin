#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "../common/common.h"
#include "../common/protocol.h"
#include "auth.h"
#include "file_ops.h"

typedef struct {
    int client_sock;
    struct sockaddr_in client_addr;
} ClientThreadArgs;

void *client_thread(void *arg);

#endif /* CLIENT_HANDLER_H */
