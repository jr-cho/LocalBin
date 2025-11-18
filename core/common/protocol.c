#include "protocol.h"
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>

/* Initialize a packet (local representation uses host order in fields) */
void init_packet(Packet *pkt, CommandType cmd, const char *data) {
    memset(pkt, 0, sizeof(Packet));
    pkt->command = (uint32_t)cmd;
    if (data) {
        size_t len = strlen(data);
        if (len > MAX_PAYLOAD) len = MAX_PAYLOAD;
        memcpy(pkt->data, data, len);
        pkt->data_length = (uint32_t)len;
    } else {
        pkt->data_length = 0;
    }
}

void xor_crypt(unsigned char *data, size_t len, 
               const unsigned char *key, size_t key_len) {
    for (size_t i = 0; i < len; i++) {
        data[i] ^= key[i % key_len];  // Each byte XORed with repeating key
    }
}

/* Send packet: write header in network order, then payload */
int send_packet(int sockfd, const Packet *pkt) {
    uint32_t net_cmd = htonl(pkt->command);
    uint32_t net_len = htonl(pkt->data_length);

    /* send header */
    if (send_all(sockfd, &net_cmd, sizeof(net_cmd)) < 0) return -1;
    if (send_all(sockfd, &net_len, sizeof(net_len)) < 0) return -1;

    /* send payload if any */
    if (pkt->data_length > 0) {
        if (send_all(sockfd, pkt->data, pkt->data_length) < 0) return -1;
    }
    return 0;
}

/* Receive packet: read header (network order), convert to host order, then payload */
int recv_packet(int sockfd, Packet *pkt) {
    uint32_t net_cmd;
    uint32_t net_len;

    if (recv_all(sockfd, &net_cmd, sizeof(net_cmd)) <= 0) return -1;
    if (recv_all(sockfd, &net_len, sizeof(net_len)) <= 0) return -1;

    pkt->command = ntohl(net_cmd);
    pkt->data_length = ntohl(net_len);

    if (pkt->data_length > 0) {
        if (pkt->data_length > MAX_PAYLOAD) return -1;
        if (recv_all(sockfd, pkt->data, pkt->data_length) <= 0) return -1;
    }
    return 0;
}

const char *command_to_string(uint32_t cmd) {
    switch (cmd) {
        case CMD_AUTH: return "AUTH";
        case CMD_UPLOAD: return "UPLOAD";
        case CMD_DOWNLOAD: return "DOWNLOAD";
        case CMD_LIST: return "LIST";
        case CMD_DELETE: return "DELETE";
        case CMD_EXIT: return "EXIT";
        case CMD_ACK: return "ACK";
        case CMD_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
