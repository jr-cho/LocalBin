#ifndef AUTH_H
#define AUTH_H

#include "../common/common.h"

#define USERS_FILE "data/users.json"
#define MAX_USERS 15

typedef struct {
    char username[USERNAME_LEN];
    char password[PASSWORD_LEN];
} UserRecord;

int load_users(void);
int authenticate_user(const char *username, const char *password);

#endif /* AUTH_H */
