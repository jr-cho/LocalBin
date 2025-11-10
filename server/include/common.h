#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>

// ================================================================
//  Common Constants
// ================================================================
#define PORT            8080
#define BUFFER_SIZE     4096
#define MAX_CLIENTS     10
#define MAX_USERNAME    32
#define MAX_PASSWORD    32

// ================================================================
//  File Paths
// ================================================================
#define LOG_FILE_PATH   "server/data/logs/server_log.txt"
#define STORAGE_PATH    "server/data/storage/"
#define USER_DB_PATH    "server/data/users.json"

#endif // COMMON_H
