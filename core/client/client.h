#ifndef CLIENT_H
#define CLIENT_H

#include "../common/common.h"
#include "../common/protocol.h"

/* Client connection context */
typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
    int is_connected;
} Client;

/* === Public API (for Python FFI) === */

/* Initialize client and connect to server (returns 0 on success) */
int client_connect(Client *c, const char *host, int port);

/* Authenticate user credentials (returns 0 on success) */
int client_auth(Client *c, const char *username, const char *password);

/* Upload file to server (returns 0 on success) */
int client_upload(Client *c, const char *username, const char *filepath);

/* Download file from server (returns 0 on success) */
int client_download(Client *c, const char *username, const char *filename, const char *save_path);

/* Disconnect gracefully */
void client_disconnect(Client *c);

#endif /* CLIENT_H */
