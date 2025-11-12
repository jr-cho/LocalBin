#include "client_handler.h"

void *client_thread(void *arg) {
    ClientThreadArgs *ctx = (ClientThreadArgs*)arg;
    int sock = ctx->client_sock;

    char msgbuf[128];
    snprintf(msgbuf, sizeof(msgbuf), "Client thread started FD=%d", sock);
    log_message("INFO", msgbuf);

    Packet req, resp;
    int authenticated = 0;
    char current_user[USERNAME_LEN] = {0};

    while (1) {
        memset(&req, 0, sizeof(req));
        if (recv_packet(sock, &req) < 0) {
            log_message("INFO", "client_thread: recv_packet failed or client disconnected");
            break;
        }

        switch (req.command) {
            case CMD_AUTH: {
                char user[USERNAME_LEN], pass[PASSWORD_LEN];
                if (sscanf(req.data, "%63[^:]:%63s", user, pass) != 2) {
                    init_packet(&resp, CMD_ERROR, "AUTH_MALFORMED");
                    send_packet(sock, &resp);
                    break;
                }
                if (authenticate_user(user, pass)) {
                    authenticated = 1;
                    strncpy(current_user, user, USERNAME_LEN - 1);
                    init_packet(&resp, CMD_ACK, "AUTH_OK");
                    send_packet(sock, &resp);
                    snprintf(msgbuf, sizeof(msgbuf), "User %s authenticated", user);
                    log_message("INFO", msgbuf);
                } else {
                    init_packet(&resp, CMD_ERROR, "AUTH_FAIL");
                    send_packet(sock, &resp);
                    log_message("WARN", "Authentication failed");
                }
                break;
            }

            case CMD_UPLOAD: {
                if (!authenticated) {
                    init_packet(&resp, CMD_ERROR, "NOT_AUTH");
                    send_packet(sock, &resp);
                    break;
                }
                if (handle_file_upload(sock, &req) == 0) {
                    init_packet(&resp, CMD_ACK, "UPLOAD_OK");
                    send_packet(sock, &resp);
                } else {
                    init_packet(&resp, CMD_ERROR, "UPLOAD_FAIL");
                    send_packet(sock, &resp);
                }
                break;
            }

            case CMD_DOWNLOAD: {
                if (!authenticated) {
                    init_packet(&resp, CMD_ERROR, "NOT_AUTH");
                    send_packet(sock, &resp);
                    break;
                }
                if (handle_file_download(sock, req.data) == 0) {
                    /* success already logged */
                } else {
                    init_packet(&resp, CMD_ERROR, "DOWNLOAD_FAIL");
                    send_packet(sock, &resp);
                }
                break;
            }

            case CMD_LIST:
                init_packet(&resp, CMD_ERROR, "LIST_NOT_IMPLEMENTED");
                send_packet(sock, &resp);
                break;

            case CMD_DELETE:
                init_packet(&resp, CMD_ERROR, "DELETE_NOT_IMPLEMENTED");
                send_packet(sock, &resp);
                break;

            case CMD_EXIT:
                log_message("INFO", "Client requested exit");
                goto end_loop;

            default:
                init_packet(&resp, CMD_ERROR, "UNKNOWN_CMD");
                send_packet(sock, &resp);
                log_message("WARN", "Unknown command");
                break;
        }
    }

end_loop:
    close(sock);
    free(ctx);
    log_message("INFO", "Client thread exiting");
    return NULL;
}
