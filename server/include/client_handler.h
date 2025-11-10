#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "common.h"
#include "logging.h"
#include "auth.h"
#include "file_ops.h"

void* handle_client(void* socket_desc);

#endif // CLIENT_HANDLER_H
