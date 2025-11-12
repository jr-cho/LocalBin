#ifndef COMMON_H
#define COMMON_H

/* Standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

/* General constants */
#define BUFFER_SIZE     4096
#define PATH_LEN        512
#define USERNAME_LEN    64
#define PASSWORD_LEN    64
#define FILE_NAME_LEN   256

/* Logging */
#define LOG_DIR         "data/logs"
#define LOG_FILE_BASE   "server"

/* Utility macros */
#define UNUSED(x)       (void)(x)

void init_logging(void);
void log_message(const char *level, const char *message);
void handle_error(const char *msg);
void get_timestamp(char *buffer, size_t len);
void get_log_filename(char *buffer, size_t len);

ssize_t send_all(int sockfd, const void *buf, size_t len);
ssize_t recv_all(int sockfd, void *buf, size_t len);

#endif /* COMMON_H */
