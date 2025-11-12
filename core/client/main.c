#include "client.h"

int main() {
    init_logging();
    Client c;

    if (client_connect(&c, "127.0.0.1", 8080) != 0) return 1;

    if (client_auth(&c, "testuser", "password") == 0) {
        client_upload(&c, "testuser", "test.txt");
        client_download(&c, "testuser", "test.txt", ".");
    }

    client_disconnect(&c);
    return 0;
}
