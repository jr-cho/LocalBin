#include "client.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>

int client_connect(Client *c, const char *host, int port) {
    if (!c) return -1;

    memset(c, 0, sizeof(Client));
    c->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (c->sockfd < 0) {
        log_message("ERROR", "client_connect: socket creation failed");
        return -1;
    }

    // Set socket timeout to prevent indefinite hanging
    struct timeval timeout;
    timeout.tv_sec = 10;  // 10 second timeout
    timeout.tv_usec = 0;
    setsockopt(c->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(c->sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    c->server_addr.sin_family = AF_INET;
    c->server_addr.sin_port = htons(port);

    // Try direct IP conversion first
    if (inet_pton(AF_INET, host, &c->server_addr.sin_addr) <= 0) {
        // If that fails, try hostname resolution
        struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char msg[256];
        snprintf(msg, sizeof(msg), "Attempting to resolve hostname: %s", host);
        log_message("INFO", msg);

        if (getaddrinfo(host, NULL, &hints, &result) != 0) {
            log_message("ERROR", "client_connect: hostname resolution failed");
            close(c->sockfd);
            return -1;
        }

        // Use the first result
        struct sockaddr_in *addr = (struct sockaddr_in *)result->ai_addr;
        c->server_addr.sin_addr = addr->sin_addr;
        freeaddrinfo(result);

        char resolved_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &c->server_addr.sin_addr, resolved_ip, INET_ADDRSTRLEN);
        snprintf(msg, sizeof(msg), "Resolved %s to %s", host, resolved_ip);
        log_message("INFO", msg);
    }

    char connect_msg[256];
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &c->server_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    snprintf(connect_msg, sizeof(connect_msg), 
             "Attempting connection to %s:%d", ip_str, port);
    log_message("INFO", connect_msg);

    if (connect(c->sockfd, (struct sockaddr*)&c->server_addr, 
                sizeof(c->server_addr)) < 0) {
        snprintf(connect_msg, sizeof(connect_msg), 
                 "client_connect: connect() failed to %s:%d - %s", 
                 ip_str, port, strerror(errno));
        log_message("ERROR", connect_msg);
        close(c->sockfd);
        return -1;
    }

    c->is_connected = 1;
    snprintf(connect_msg, sizeof(connect_msg), 
             "Client successfully connected to %s:%d", ip_str, port);
    log_message("INFO", connect_msg);
    return 0;
}

// Rest of the functions remain the same...
int client_auth(Client *c, const char *username, const char *password) {
    if (!c || !c->is_connected) return -1;

    char data[USERNAME_LEN + PASSWORD_LEN + 8];
    snprintf(data, sizeof(data), "%s:%s", username, password);

    Packet p;
    init_packet(&p, CMD_AUTH, data);
    if (send_packet(c->sockfd, &p) < 0) return -1;

    Packet resp;
    if (recv_packet(c->sockfd, &resp) < 0) return -1;

    if (resp.command == CMD_ACK && strstr(resp.data, "AUTH_OK")) {
        log_message("INFO", "Authentication successful");
        return 0;
    }

    log_message("WARN", "Authentication failed");
    return -1;
}

int client_upload(Client *c, const char *username, const char *filepath) {
    if (!c || !c->is_connected) return -1;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        log_message("ERROR", "client_upload: cannot open file");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    size_t filesize = (size_t)ftell(fp);
    rewind(fp);

    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;

    char header[USERNAME_LEN + FILE_NAME_LEN + 64];
    snprintf(header, sizeof(header), "%s:%s:%zu", username, filename, filesize);

    Packet p;
    init_packet(&p, CMD_UPLOAD, header);
    if (send_packet(c->sockfd, &p) < 0) {
        fclose(fp);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t n;
    size_t sent = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send_all(c->sockfd, buffer, n) < 0) {
            fclose(fp);
            log_message("ERROR", "client_upload: send_all failed");
            return -1;
        }
        sent += n;
    }
    fclose(fp);

    Packet resp;
    if (recv_packet(c->sockfd, &resp) == 0 && resp.command == CMD_ACK) {
        log_message("INFO", "Upload completed successfully");
        return 0;
    }
    log_message("WARN", "Upload failed or not acknowledged");
    return -1;
}

int client_download(Client *c, const char *username, const char *filename, const char *save_path) {
    if (!c || !c->is_connected) return -1;

    char header[USERNAME_LEN + FILE_NAME_LEN + 8];
    snprintf(header, sizeof(header), "%s:%s", username, filename);

    Packet req;
    init_packet(&req, CMD_DOWNLOAD, header);
    if (send_packet(c->sockfd, &req) < 0) return -1;

    Packet ack;
    if (recv_packet(c->sockfd, &ack) < 0) return -1;
    if (ack.command != CMD_ACK) {
        log_message("ERROR", "client_download: server did not ACK");
        return -1;
    }

    size_t filesize = strtoull(ack.data, NULL, 10);
    if (filesize == 0) {
        log_message("WARN", "client_download: empty file or error");
        return -1;
    }

    char fullpath[PATH_LEN];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", save_path, filename);
    FILE *fp = fopen(fullpath, "wb");
    if (!fp) {
        log_message("ERROR", "client_download: cannot open save path");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t total = 0;
    ssize_t r;
    while (total < filesize) {
        r = recv(c->sockfd, buffer, BUFFER_SIZE, 0);
        if (r <= 0) {
            log_message("ERROR", "client_download: recv failed");
            fclose(fp);
            return -1;
        }
        fwrite(buffer, 1, (size_t)r, fp);
        total += (size_t)r;
    }
    fclose(fp);

    log_message("INFO", "File download complete");
    return 0;
}

void client_disconnect(Client *c) {
    if (!c || !c->is_connected) return;

    Packet p;
    init_packet(&p, CMD_EXIT, "EXIT");
    send_packet(c->sockfd, &p);

    close(c->sockfd);
    c->is_connected = 0;
    log_message("INFO", "Client disconnected");
}
