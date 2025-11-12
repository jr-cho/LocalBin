#include "client.h"
#include <arpa/inet.h>
#include <fcntl.h>

int client_connect(Client *c, const char *host, int port) {
    if (!c) return -1;

    memset(c, 0, sizeof(Client));
    c->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (c->sockfd < 0) {
        log_message("ERROR", "client_connect: socket creation failed");
        return -1;
    }

    c->server_addr.sin_family = AF_INET;
    c->server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &c->server_addr.sin_addr) <= 0) {
        log_message("ERROR", "client_connect: invalid server address");
        close(c->sockfd);
        return -1;
    }

    if (connect(c->sockfd, (struct sockaddr*)&c->server_addr, sizeof(c->server_addr)) < 0) {
        log_message("ERROR", "client_connect: connect() failed");
        close(c->sockfd);
        return -1;
    }

    c->is_connected = 1;
    log_message("INFO", "Client connected to server");
    return 0;
}

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
