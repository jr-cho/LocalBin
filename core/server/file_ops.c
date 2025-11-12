#include "file_ops.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

static void ensure_base_dir(void) {
    struct stat st;
    if (stat(STORAGE_BASE, &st) == -1) {
        mkdir(STORAGE_BASE, 0755);
    }
}

static void ensure_user_dir(const char *user) {
    char path[PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", STORAGE_BASE, user);
    struct stat st;
    if (stat(path, &st) == -1) {
        ensure_base_dir();
        mkdir(path, 0755);
    }
}

static void build_path(char *dest, size_t len, const char *user, const char *filename) {
    snprintf(dest, len, "%s/%s/%s", STORAGE_BASE, user, filename);
}

int handle_file_upload(int sockfd, Packet *initial_request) {
    char user[USERNAME_LEN] = {0};
    char filename[FILE_NAME_LEN] = {0};
    size_t filesize = 0;

    if (sscanf(initial_request->data, "%63[^:]:%255[^:]:%zu", user, filename, &filesize) != 3) {
        log_message("ERROR", "handle_file_upload: bad header");
        return -1;
    }

    ensure_user_dir(user);

    char fullpath[PATH_LEN];
    build_path(fullpath, sizeof(fullpath), user, filename);

    FILE *fp = fopen(fullpath, "wb");
    if (!fp) {
        log_message("ERROR", "handle_file_upload: fopen failed");
        return -1;
    }

    size_t total = 0;
    char buf[CHUNK_SIZE];
    while (total < filesize) {
        ssize_t r = recv(sockfd, buf, CHUNK_SIZE, 0);
        if (r <= 0) {
            if (r == 0) log_message("WARN", "handle_file_upload: client closed");
            else log_message("ERROR", "handle_file_upload: recv error");
            fclose(fp);
            return -1;
        }
        fwrite(buf, 1, (size_t)r, fp);
        total += (size_t)r;
    }

    fclose(fp);
    char msg[256];
    snprintf(msg, sizeof(msg), "Uploaded %s for %s (%zu bytes)", filename, user, total);
    log_message("INFO", msg);
    return 0;
}

int handle_file_download(int sockfd, const char *data) {
    char user[USERNAME_LEN] = {0};
    char filename[FILE_NAME_LEN] = {0};

    if (sscanf(data, "%63[^:]:%255s", user, filename) != 2) {
        log_message("ERROR", "handle_file_download: bad request");
        return -1;
    }

    char fullpath[PATH_LEN];
    build_path(fullpath, sizeof(fullpath), user, filename);

    FILE *fp = fopen(fullpath, "rb");
    if (!fp) {
        log_message("WARN", "handle_file_download: file not found");
        Packet err;
        init_packet(&err, CMD_ERROR, "FILE_NOT_FOUND");
        send_packet(sockfd, &err);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    size_t filesize = (size_t)ftell(fp);
    rewind(fp);

    char header[64];
    snprintf(header, sizeof(header), "%zu", filesize);
    Packet ack;
    init_packet(&ack, CMD_ACK, header);
    if (send_packet(sockfd, &ack) < 0) {
        fclose(fp);
        log_message("ERROR", "handle_file_download: send_packet failed");
        return -1;
    }

    char buf[CHUNK_SIZE];
    size_t sent = 0;
    size_t n;
    while ((n = fread(buf, 1, CHUNK_SIZE, fp)) > 0) {
        if (send_all(sockfd, buf, n) < 0) {
            fclose(fp);
            log_message("ERROR", "handle_file_download: send_all failed");
            return -1;
        }
        sent += n;
    }

    fclose(fp);
    char msg[256];
    snprintf(msg, sizeof(msg), "Sent %s to client (%zu bytes)", filename, sent);
    log_message("INFO", msg);
    return 0;
}

void cleanup_user_data() {
    const char *storage_dir = "data/storage";
    const char *user_file = "data/users.json";

    DIR *dir = opendir(storage_dir);
    if (dir) {
        struct dirent *entry;
        char path[PATH_MAX];

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            snprintf(path, sizeof(path), "%s/%s", storage_dir, entry->d_name);

            char cmd[PATH_MAX * 2];
            snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
            system(cmd);

            char msg[PATH_MAX + 64];
            snprintf(msg, sizeof(msg), "Deleted user directory: %s", path);
            log_message("INFO", msg);
        }
        closedir(dir);
    }

    if (remove(user_file) == 0) {
        log_message("INFO", "Removed users.json (user profiles cleared).");
    } else {
        log_message("WARN", "Could not remove users.json (missing or locked).");
    }

    log_message("INFO", "All user data has been cleared successfully.");
}
