#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static void ensure_log_dir(void) {
    struct stat st;
    if (stat(LOG_DIR, &st) == -1) {
        mkdir(LOG_DIR, 0755);
    }
}

void get_timestamp(char *buffer, size_t len) {
    time_t now = time(NULL);
    struct tm tbuf;
    localtime_r(&now, &tbuf);
    strftime(buffer, len, "%Y-%m-%d %H:%M:%S", &tbuf);
}

void get_log_filename(char *buffer, size_t len) {
    time_t now = time(NULL);
    struct tm tbuf;
    localtime_r(&now, &tbuf);
    snprintf(buffer, len, "%s/%s-%04d-%02d-%02d.log",
             LOG_DIR, LOG_FILE_BASE,
             tbuf.tm_year + 1900, tbuf.tm_mon + 1, tbuf.tm_mday);
}

void init_logging(void) {
    ensure_log_dir();
    log_message("INFO", "Logging initialized");
}

void log_message(const char *level, const char *message) {
    pthread_mutex_lock(&log_mutex);

    ensure_log_dir();

    char filename[PATH_LEN];
    get_log_filename(filename, sizeof(filename));

    FILE *fp = fopen(filename, "a");
    if (!fp) {
        /* If we can't open the file, at least print to stderr */
        fprintf(stderr, "log_message: fopen failed: %s\n", strerror(errno));
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    char ts[64];
    get_timestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [%s] %s\n", ts, level, message);
    fclose(fp);

    pthread_mutex_unlock(&log_mutex);
}

void handle_error(const char *msg) {
    perror(msg);
    log_message("ERROR", msg);
    exit(EXIT_FAILURE);
}

ssize_t send_all(int sockfd, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = (const char*)buf;
    while (total < len) {
        ssize_t n = send(sockfd, p + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    return (ssize_t)total;
}

ssize_t recv_all(int sockfd, void *buf, size_t len) {
    size_t total = 0;
    char *p = (char*)buf;
    while (total < len) {
        ssize_t n = recv(sockfd, p + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    return (ssize_t)total;
}
