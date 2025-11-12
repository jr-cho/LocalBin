#include "auth.h"
#include <pthread.h>

static UserRecord users[MAX_USERS];
static int users_count = 0;
static pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

/* helper to strip quotes and whitespace */
static void sanitize_field(char *s) {
    char *src = s, *dst = s;
    while (*src) {
        if (*src != '"' && *src != ' ' && *src != '\t' && *src != '\r' && *src != '\n')
            *dst++ = *src;
        src++;
    }
    *dst = '\0';
}

int load_users(void) {
    pthread_mutex_lock(&users_mutex);
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) {
        log_message("ERROR", "load_users: cannot open users.json");
        pthread_mutex_unlock(&users_mutex);
        return -1;
    }

    char buf[512];
    int count = 0;
    while (fgets(buf, sizeof(buf), fp) && count < MAX_USERS) {
        char *p = strchr(buf, '"');
        if (!p) continue;
        char *q = strchr(p + 1, '"');
        if (!q) continue;

        char username[USERNAME_LEN] = {0};
        size_t un_len = (size_t)(q - p - 1);
        if (un_len >= sizeof(username)) un_len = sizeof(username) - 1;
        strncpy(username, p + 1, un_len);

        /* find next quoted string (password) */
        char *r = strchr(q + 1, '"');
        if (!r) continue;
        char *s = strchr(r + 1, '"');
        if (!s) continue;

        char password[PASSWORD_LEN] = {0};
        size_t pw_len = (size_t)(s - r - 1);
        if (pw_len >= sizeof(password)) pw_len = sizeof(password) - 1;
        strncpy(password, r + 1, pw_len);

        sanitize_field(username);
        sanitize_field(password);

        strncpy(users[count].username, username, USERNAME_LEN - 1);
        strncpy(users[count].password, password, PASSWORD_LEN - 1);
        count++;
    }

    fclose(fp);
    users_count = count;

    char msg[128];
    snprintf(msg, sizeof(msg), "Loaded %d users", users_count);
    log_message("INFO", msg);

    pthread_mutex_unlock(&users_mutex);
    return users_count;
}

int authenticate_user(const char *username, const char *password) {
    if (users_count == 0) {
        if (load_users() <= 0) {
            log_message("ERROR", "authenticate_user: no users loaded");
            return 0;
        }
    }

    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < users_count; ++i) {
        if (strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0) {
            pthread_mutex_unlock(&users_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&users_mutex);
    return 0;
}
