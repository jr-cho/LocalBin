#include "../include/logging.h"

/*
 * ==============================================================
 *  Function: log_event
 *  Purpose : Append timestamped log messages to server_log.txt
 * ==============================================================
 */
void log_event(const char* event, const char* detail) {
    FILE* fp = fopen(LOG_FILE_PATH, "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm* t = localtime(&now);

    fprintf(fp, "[%02d-%02d-%04d %02d:%02d:%02d] %-10s | %s\n",
            t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
            t->tm_hour, t->tm_min, t->tm_sec,
            event, detail);

    fclose(fp);
}

