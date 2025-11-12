#ifndef FILE_OPS_H
#define FILE_OPS_H

#include "../common/common.h"
#include "../common/protocol.h"

#define STORAGE_BASE "data/storage"
#define CHUNK_SIZE 4096

int handle_file_upload(int sockfd, Packet *initial_request);
int handle_file_download(int sockfd, const char *data);

void cleanup_user_data(void);

#endif /* FILE_OPS_H */
