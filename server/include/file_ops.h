#ifndef FILE_OPS_H
#define FILE_OPS_H

#include "common.h"
#include "logging.h"

void handle_upload(int client_sock, const char* filename);
void handle_download(int client_sock, const char* filename);
void handle_delete(int client_sock, const char* filename);
void handle_dir(int client_sock);

#endif // FILE_OPS_H
