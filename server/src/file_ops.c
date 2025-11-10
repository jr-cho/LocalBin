#include "../include/file_ops.h"

/*
 * ===============================
 *  File Handling Stubs 
 * ===============================
 */

void handle_upload(int client_sock, const char* filename) {
    log_event("UPLOAD", filename);
    send(client_sock, "UPLOAD not yet implemented\n", 27, 0);
}

void handle_download(int client_sock, const char* filename) {
    log_event("DOWNLOAD", filename);
    send(client_sock, "DOWNLOAD not yet implemented\n", 29, 0);
}

void handle_delete(int client_sock, const char* filename) {
    log_event("DELETE", filename);
    send(client_sock, "DELETE not yet implemented\n", 27, 0);
}

void handle_dir(int client_sock) {
    log_event("DIR", "Directory listing requested");
    send(client_sock, "DIR not yet implemented\n", 24, 0);
}
