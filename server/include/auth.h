#ifndef AUTH_H
#define AUTH_H

#include "common.h"

bool authenticate_user(const char *username, const char *password);
void hash_password(const char* input, char* output);

#endif // AUTH_H
