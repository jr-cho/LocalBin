#include "../include/auth.h"

/*
 * ==============================================================
 *  Function: simple_hash
 *  Purpose : Minimal obfuscation for password storage
 * ==============================================================
 */
void simple_hash(const char* input, char* output) {
    size_t len = strlen(input);
    for (size_t i = 0; i < len; i++) {
        output[i] = input[i] ^ 0x5A; 
    }
    output[len] = '\0';
}

/*
 * ==============================================================
 *  Function: authenticate_user
 *  Purpose : Validate credentials from users.json
 * ==============================================================
 */
bool authenticate_user(const char* username, const char* password) {
    FILE* fp = fopen(USER_DB_PATH, "r");
    if (!fp) {
        perror("[ERROR] Unable to open users.json");
        return false;
    }

    // Read entire file into memory
    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* data = malloc(length + 1);
    if (!data) {
        fclose(fp);
        return false;
    }
    fread(data, 1, length, fp);
    data[length] = '\0';
    fclose(fp);

    // Hash the incoming password for comparison
    char hashed[MAX_PASSWORD];
    simple_hash(password, hashed);

    // Search for username and password in the JSON string
    char search_user[64];
    snprintf(search_user, sizeof(search_user), "\"username\": \"%s\"", username);

    char search_pass[64];
    snprintf(search_pass, sizeof(search_pass), "\"password\": \"%s\"", hashed);

    bool match = false;

    char* user_pos = strstr(data, search_user);
    if (user_pos) {
        char* pass_pos = strstr(user_pos, search_pass);
        if (pass_pos) {
            match = true;
        }
    }

    free(data);
    return match;
}
