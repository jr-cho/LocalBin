#include "client.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <netinet/tcp.h>

int client_connect(Client *c, const char *host, int port) {
    if (!c) return -1;

    memset(c, 0, sizeof(Client));
    c->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (c->sockfd < 0) {
        log_message("ERROR", "client_connect: socket creation failed");
        return -1;
    }

    // Set TCP_NODELAY to disable Nagle's algorithm for better interactive performance
    int flag = 1;
    if (setsockopt(c->sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        log_message("WARN", "client_connect: could not set TCP_NODELAY");
    }

    // Set socket timeout to prevent indefinite hanging
    struct timeval timeout;
    timeout.tv_sec = 10;  // 10 second timeout
    timeout.tv_usec = 0;
    if (setsockopt(c->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message("WARN", "client_connect: could not set SO_RCVTIMEO");
    }
    if (setsockopt(c->sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message("WARN", "client_connect: could not set SO_SNDTIMEO");
    }

    // Set SO_KEEPALIVE to detect dead connections
    int keepalive = 1;
    if (setsockopt(c->sockfd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        log_message("WARN", "client_connect: could not set SO_KEEPALIVE");
    }

    c->server_addr.sin_family = AF_INET;
    c->server_addr.sin_port = htons(port);

    // Try direct IP conversion first
    if (inet_pton(AF_INET, host, &c->server_addr.sin_addr) <= 0) {
        // If that fails, try hostname resolution
        struct addrinfo hints, *result, *rp;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        char msg[256];
        snprintf(msg, sizeof(msg), "Attempting to resolve hostname: %s", host);
        log_message("INFO", msg);

        int res = getaddrinfo(host, NULL, &hints, &result);
        if (res != 0) {
            snprintf(msg, sizeof(msg), "client_connect: hostname resolution failed: %s", gai_strerror(res));
            log_message("ERROR", msg);
            close(c->sockfd);
            return -1;
        }

        // Try each address until we successfully connect
        int connected = 0;
        for (rp = result; rp != NULL; rp = rp->ai_next) {
            struct sockaddr_in *addr = (struct sockaddr_in *)rp->ai_addr;
            c->server_addr.sin_addr = addr->sin_addr;

            char resolved_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &c->server_addr.sin_addr, resolved_ip, INET_ADDRSTRLEN);
            snprintf(msg, sizeof(msg), "Resolved %s to %s, attempting connection...", host, resolved_ip);
            log_message("INFO", msg);

            if (connect(c->sockfd, (struct sockaddr*)&c->server_addr, sizeof(c->server_addr)) == 0) {
                connected = 1;
                break;
            }
        }
        freeaddrinfo(result);

        if (!connected) {
            snprintf(msg, sizeof(msg), "client_connect: could not connect to any resolved address for %s", host);
            log_message("ERROR", msg);
            close(c->sockfd);
            return -1;
        }
    } else {
        // Direct IP address connection
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
    }

    c->is_connected = 1;
    
    char success_msg[256];
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &c->server_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    snprintf(success_msg, sizeof(success_msg), 
             "Client successfully connected to %s:%d", ip_str, port);
    log_message("INFO", success_msg);
    
    return 0;
}

int client_auth(Client *c, const char *username, const char *password) {
    if (!c || !c->is_connected) {
        log_message("ERROR", "client_auth: not connected");
        return -1;
    }

    char data[USERNAME_LEN + PASSWORD_LEN + 8];
    snprintf(data, sizeof(data), "%s:%s", username, password);

    Packet p;
    init_packet(&p, CMD_AUTH, data);
    if (send_packet(c->sockfd, &p) < 0) {
        log_message("ERROR", "client_auth: send_packet failed");
        return -1;
    }

    Packet resp;
    if (recv_packet(c->sockfd, &resp) < 0) {
        log_message("ERROR", "client_auth: recv_packet failed");
        return -1;
    }

    if (resp.command == CMD_ACK && strstr(resp.data, "AUTH_OK")) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Authentication successful for user: %s", username);
        log_message("INFO", msg);
        return 0;
    }

    log_message("WARN", "Authentication failed - invalid credentials");
    return -1;
}

int client_upload(Client *c, const char *username, const char *filepath) {
    if (!c || !c->is_connected) {
        log_message("ERROR", "client_upload: not connected");
        return -1;
    }

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        char msg[256];
        snprintf(msg, sizeof(msg), "client_upload: cannot open file: %s", filepath);
        log_message("ERROR", msg);
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
        log_message("ERROR", "client_upload: send_packet failed for header");
        fclose(fp);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t n;
    size_t sent = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        ssize_t result = send_all(c->sockfd, buffer, n);
        if (result < 0 || (size_t)result != n) {
            char msg[128];
            snprintf(msg, sizeof(msg), "client_upload: send_all failed (sent %zu/%zu)", sent, filesize);
            log_message("ERROR", msg);
            fclose(fp);
            return -1;
        }
        sent += n;
    }
    fclose(fp);

    char msg[256];
    snprintf(msg, sizeof(msg), "client_upload: sent %zu bytes, waiting for ACK", sent);
    log_message("INFO", msg);

    Packet resp;
    if (recv_packet(c->sockfd, &resp) == 0 && resp.command == CMD_ACK) {
        snprintf(msg, sizeof(msg), "Upload completed successfully: %s (%zu bytes)", filename, sent);
        log_message("INFO", msg);
        return 0;
    }
    
    log_message("WARN", "Upload failed or not acknowledged by server");
    return -1;
}

int client_download(Client *c, const char *username, const char *filename, const char *save_path) {
    if (!c || !c->is_connected) {
        log_message("ERROR", "client_download: not connected");
        return -1;
    }

    char header[USERNAME_LEN + FILE_NAME_LEN + 8];
    snprintf(header, sizeof(header), "%s:%s", username, filename);

    Packet req;
    init_packet(&req, CMD_DOWNLOAD, header);
    if (send_packet(c->sockfd, &req) < 0) {
        log_message("ERROR", "client_download: send_packet failed");
        return -1;
    }

    Packet ack;
    if (recv_packet(c->sockfd, &ack) < 0) {
        log_message("ERROR", "client_download: recv_packet failed");
        return -1;
    }
    
    if (ack.command != CMD_ACK) {
        char msg[256];
        snprintf(msg, sizeof(msg), "client_download: server error: %s", ack.data);
        log_message("ERROR", msg);
        return -1;
    }

    size_t filesize = strtoull(ack.data, NULL, 10);
    if (filesize == 0) {
        log_message("WARN", "client_download: empty file or parse error");
        return -1;
    }

    char fullpath[PATH_LEN];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", save_path, filename);
    FILE *fp = fopen(fullpath, "wb");
    if (!fp) {
        char msg[256];
        snprintf(msg, sizeof(msg), "client_download: cannot open save path: %s", fullpath);
        log_message("ERROR", msg);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t total = 0;
    ssize_t r;
    
    char msg[256];
    snprintf(msg, sizeof(msg), "client_download: receiving %zu bytes", filesize);
    log_message("INFO", msg);
    
    while (total < filesize) {
        size_t to_read = (filesize - total < BUFFER_SIZE) ? (filesize - total) : BUFFER_SIZE;
        r = recv(c->sockfd, buffer, to_read, 0);
        if (r <= 0) {
            snprintf(msg, sizeof(msg), "client_download: recv failed after %zu bytes", total);
            log_message("ERROR", msg);
            fclose(fp);
            return -1;
        }
        fwrite(buffer, 1, (size_t)r, fp);
        total += (size_t)r;
    }
    fclose(fp);

    snprintf(msg, sizeof(msg), "File download complete: %s (%zu bytes)", filename, total);
    log_message("INFO", msg);
    return 0;
}

void client_disconnect(Client *c) {
    if (!c || !c->is_connected) return;

    Packet p;
    init_packet(&p, CMD_EXIT, "EXIT");
    send_packet(c->sockfd, &p);

    close(c->sockfd);
    c->is_connected = 0;
    log_message("INFO", "Client disconnected gracefully");
}
