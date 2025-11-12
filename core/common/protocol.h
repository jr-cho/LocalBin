#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "../common/common.h"

/* Command type identifiers */
typedef enum {
    CMD_UNKNOWN = 0,
    CMD_AUTH    = 1,
    CMD_UPLOAD  = 2,
    CMD_DOWNLOAD= 3,
    CMD_LIST    = 4,
    CMD_DELETE  = 5,
    CMD_EXIT    = 6,
    CMD_ACK     = 7,
    CMD_ERROR   = 8
} CommandType;

#define MAX_PAYLOAD (BUFFER_SIZE)

typedef struct {
    uint32_t command;      /* network order when sent */
    uint32_t data_length;  /* network order when sent */
    char data[MAX_PAYLOAD];
} Packet;

/* helpers */
void init_packet(Packet *pkt, CommandType cmd, const char *data);
int send_packet(int sockfd, const Packet *pkt);
int recv_packet(int sockfd, Packet *pkt);
const char *command_to_string(uint32_t cmd);

#endif /* PROTOCOL_H */
