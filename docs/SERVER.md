# LocalBin Server Architecture - Complete Explanation

## Overview

The server is a **multi-threaded TCP server** that:
1. Listens for incoming client connections on a port
2. Spawns a new thread for each client
3. Handles authentication and file operations
4. Logs all activity to daily log files

**Entry point**: `core/server/main.c` → `start_server()` in `server.c` → spawns `client_thread()` for each connection

---

# File: `core/server/main.c`

## What It Does
Entry point of the server application. Handles command-line arguments and signal handlers.

```c
#include "server.h"
#include "../common/common.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
```
Standard headers for server functionality and signal handling.

---

## Function: `handle_signal()`

```c
void handle_signal(int sig) {
    printf("\n[INFO] Caught signal %d, closing server...\n", sig);
    server_running = 0;
}
```

**What it does**:
- Called when the process receives SIGINT (Ctrl+C) or SIGTERM (termination)
- Sets global flag `server_running = 0` to break the accept loop
- Prints to console (not logged to file for speed)

**Example**: User presses Ctrl+C
```
Server is running...
^C
[INFO] Caught signal 2, closing server...
```

---

## Function: `main()`

```c
int main(int argc, char *argv[]) {
    int port = 8080;
    if (argc > 1)
        port = atoi(argv[1]);
```

**Parse command-line arguments**:
- If user runs `./server 9000`, port becomes 9000
- Otherwise defaults to 8080
- `atoi()` = "ASCII to integer" (convert "9000" string → 9000 int)

```c
    printf("[INFO] Starting LocalBin server on port %d...\n", port);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
```

**Register signal handlers**:
- `signal(SIGINT, ...)` = when Ctrl+C is pressed, call `handle_signal()`
- `signal(SIGTERM, ...)` = when `kill -TERM` is sent, call `handle_signal()`

```c
    init_logging();
```

**Initialize logging system** (from `common.c`):
- Creates `data/logs/` directory if it doesn't exist
- Logs first message: "Logging initialized"

```c
    printf("[INFO] Waiting for client connections...\n");
    start_server(port);
```

**Start the main server loop** (see `server.c` below)

```c
    log_message("INFO", "Server shutting down...");
    cleanup_user_data();
    log_message("INFO", "Cleanup complete. Goodbye.");
}
```

**Cleanup on exit**:
- `cleanup_user_data()` = delete all user files and users.json (see `file_ops.c`)
- Log final messages

---

---

# File: `core/server/server.c`

## Global Variables

```c
volatile int server_running = 1;
static int listen_sock = -1;
```

**`server_running`**:
- Global flag that controls the accept loop
- Set to 0 by signal handler to initiate graceful shutdown
- `volatile` tells compiler not to optimize it (it can change unexpectedly via signal)

**`listen_sock`**:
- File descriptor for the listening socket
- Stored globally so signal handler can close it

---

## Function: `sigint_handler()`

```c
static void sigint_handler(int sig) {
    UNUSED(sig);
    log_message("INFO", "SIGINT received; shutting down server");
    server_running = 0;
    if (listen_sock != -1) close(listen_sock);
}
```

**What it does**:
- `UNUSED(sig)` = macro to tell compiler we intentionally ignore the parameter
- Set `server_running = 0` to stop accepting new connections
- Close the listening socket to unblock the `accept()` call
- Log to file

**Why close the socket?**:
- If you just set `server_running = 0`, the `accept()` call is still blocking
- Closing the socket makes `accept()` return with an error
- This allows the program to exit immediately instead of waiting for the next connection

---

## Function: `start_server()`

### Initialization Phase

```c
int start_server(int port) {
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    init_logging();
    log_message("INFO", "Server initializing");

    signal(SIGINT, sigint_handler);
    server_running = 1;
```

**Setup**:
- `addr` = structure to hold server listening address
- `client_addr` = structure to hold client's address (populated by `accept()`)
- `socklen_t` = unsigned integer type for socket address length
- Register SIGINT handler (Ctrl+C)
- Set `server_running = 1` flag

```c
    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        handle_error("socket");
        return -1;
    }
```

**Create listening socket**:
- `AF_INET` = IPv4
- `SOCK_STREAM` = TCP
- `0` = default protocol (IPPROTO_TCP)
- `socket()` returns a file descriptor
- On failure, log and exit

```c
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

**Allow socket reuse**:
- Normally, closing a socket keeps it in TIME_WAIT state for 60+ seconds
- This prevents the port from being reused immediately (for robustness)
- `SO_REUSEADDR` tells OS: allow binding to this port even if it's in TIME_WAIT
- **Why**: Allows quick server restarts during development

```c
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)port);
```

**Configure server address**:
- `memset()` = zero-out the structure
- `sin_family` = IPv4
- `sin_addr.s_addr = INADDR_ANY` = listen on all network interfaces (0.0.0.0)
  - Allows connections from localhost, LAN, or WAN (depending on firewall)
- `sin_port = htons(port)` = convert port to network byte order

### Binding Phase

```c
    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        handle_error("bind");
        close(listen_sock);
        return -1;
    }
```

**Bind socket to address**:
- `bind()` = associate socket with IP:port
- If this fails, it means:
  - Port is already in use
  - Insufficient permissions (port < 1024 requires root)
  - Invalid address
- Close socket and return -1 on failure

### Listening Phase

```c
    if (listen(listen_sock, SERVER_BACKLOG) < 0) {
        handle_error("listen");
        close(listen_sock);
        return -1;
    }
```

**Mark socket as listening**:
- `SERVER_BACKLOG = 16` (from `server.h`)
- This means: allow up to 16 connection requests to queue up
- Once we call `accept()`, we dequeue one from the queue
- If 16 clients are waiting and the 17th connects, it gets rejected

### Startup Message

```c
    char buf[128];
    snprintf(buf, sizeof(buf), "Server listening on port %d", port);
    log_message("INFO", buf);
    printf("[SERVER] %s\n", buf);
```

**Log startup**:
- Log to file and print to console

### Main Accept Loop

```c
    while (server_running) {
        int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            if (errno == EINTR) continue;
            log_message("ERROR", "accept failed");
            continue;
        }
```

**Accept incoming connections**:
- `accept()` = **blocking call** that waits for a client to connect
- Returns a new file descriptor for the client socket
- `client_addr` = filled in with client's IP and port
- `client_len` = on input, size of `client_addr`; on output, size actually filled in

**Error handling**:
- If `accept()` returns < 0, something went wrong
- `errno == EINTR` = interrupted by signal (non-fatal, retry)
- Other errors = log and continue

```c
        ClientThreadArgs *args = malloc(sizeof(ClientThreadArgs));
        if (!args) {
            log_message("ERROR", "malloc failed for client args");
            close(client_sock);
            continue;
        }
        args->client_sock = client_sock;
        args->client_addr = client_addr;
```

**Allocate thread arguments**:
- Create a heap structure to pass data to the new thread
- Thread can't access local variables of this function (they're on the stack)
- Must pass data via heap-allocated structure
- Store the client socket FD and address

```c
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, args) != 0) {
            log_message("ERROR", "pthread_create failed");
            close(client_sock);
            free(args);
            continue;
        }
        pthread_detach(tid);
```

**Spawn thread for client**:
- `pthread_create()` = create a new thread
  - `&tid` = thread ID (returned)
  - `NULL` = default thread attributes
  - `client_thread` = function to run
  - `args` = argument to pass to that function
- On failure, close socket and free memory
- `pthread_detach(tid)` = tell OS to clean up thread resources automatically when it exits
  - (Otherwise, thread becomes a "zombie" until `pthread_join()` is called)

```c
        snprintf(buf, sizeof(buf), "Accepted %s:%d",
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        log_message("INFO", buf);
    }
```

**Log the connection**:
- `inet_ntoa()` = convert binary IP to string ("192.168.1.1")
- `ntohs()` = convert port from network byte order to host order
- Example: "Accepted 192.168.1.100:54321"

### Shutdown Phase

```c
    if (listen_sock != -1) {
        close(listen_sock);
        listen_sock = -1;
    }
    log_message("INFO", "Server stopped");
    return 0;
}
```

**Cleanup**:
- Close listening socket
- Set to -1 to indicate it's closed
- Log final message

---

---

# File: `core/server/client_handler.c`

## Function: `client_thread()`

This function runs in a separate thread for each connected client.

```c
void *client_thread(void *arg) {
    ClientThreadArgs *ctx = (ClientThreadArgs*)arg;
    int sock = ctx->client_sock;

    char msgbuf[128];
    snprintf(msgbuf, sizeof(msgbuf), "Client thread started FD=%d", sock);
    log_message("INFO", msgbuf);
```

**Thread initialization**:
- Cast argument to proper type
- Extract socket FD
- Log thread startup with socket descriptor

```c
    Packet req, resp;
    int authenticated = 0;
    char current_user[USERNAME_LEN] = {0};
```

**Initialize client state**:
- `req`, `resp` = buffers for incoming/outgoing packets
- `authenticated` = flag; 0 = not authenticated, 1 = authenticated
- `current_user` = username of authenticated user (used for file ops)

### Main Command Loop

```c
    while (1) {
        memset(&req, 0, sizeof(req));
        if (recv_packet(sock, &req) < 0) {
            log_message("INFO", "client_thread: recv_packet failed or client disconnected");
            break;
        }
```

**Receive command**:
- Zero-out request buffer
- `recv_packet()` = deserialize packet from socket (see protocol.c)
- If returns < 0, connection died or client disconnected
- Break out of loop

```c
        switch (req.command) {
```

**Command dispatcher**:
- Handle different commands differently

---

### Command: `CMD_AUTH`

```c
            case CMD_AUTH: {
                char user[USERNAME_LEN], pass[PASSWORD_LEN];
                if (sscanf(req.data, "%63[^:]:%63s", user, pass) != 2) {
                    init_packet(&resp, CMD_ERROR, "AUTH_MALFORMED");
                    send_packet(sock, &resp);
                    break;
                }
```

**Parse credentials**:
- Data format: "username:password"
- `sscanf()` with format `%63[^:]:%63s`:
  - `%63[^:]` = read up to 63 characters until ':' is found (username)
  - `:` = expect a colon
  - `%63s` = read up to 63 characters (password)
- If parsing fails (returns ≠ 2), send error packet

```c
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
```

**Authenticate**:
- Call `authenticate_user()` (see auth.c below)
- If successful:
  - Set `authenticated = 1`
  - Save username in `current_user`
  - Send CMD_ACK with "AUTH_OK"
  - Log success
- If failed:
  - Send CMD_ERROR with "AUTH_FAIL"
  - Log warning

---

### Command: `CMD_UPLOAD`

```c
            case CMD_UPLOAD: {
                if (!authenticated) {
                    init_packet(&resp, CMD_ERROR, "NOT_AUTH");
                    send_packet(sock, &resp);
                    break;
                }
```

**Check authentication**:
- Only authenticated users can upload
- If not authenticated, send error and skip

```c
                if (handle_file_upload(sock, &req) == 0) {
                    init_packet(&resp, CMD_ACK, "UPLOAD_OK");
                    send_packet(sock, &resp);
                } else {
                    init_packet(&resp, CMD_ERROR, "UPLOAD_FAIL");
                    send_packet(sock, &resp);
                }
                break;
            }
```

**Handle upload**:
- Call `handle_file_upload()` (see file_ops.c below)
- If returns 0 (success), send ACK
- Otherwise send error

---

### Command: `CMD_DOWNLOAD`

```c
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
```

**Handle download**:
- Check authentication
- Call `handle_file_download()` (see file_ops.c)
- If it fails, send error
- (Note: successful download doesn't send extra packet; file data is already streamed)

---

### Command: `CMD_EXIT`

```c
            case CMD_EXIT:
                log_message("INFO", "Client requested exit");
                goto end_loop;
```

**Graceful disconnect**:
- Jump to end of loop to close connection

---

### Other Commands

```c
            case CMD_LIST:
                init_packet(&resp, CMD_ERROR, "LIST_NOT_IMPLEMENTED");
                send_packet(sock, &resp);
                break;

            case CMD_DELETE:
                init_packet(&resp, CMD_ERROR, "DELETE_NOT_IMPLEMENTED");
                send_packet(sock, &resp);
                break;

            default:
                init_packet(&resp, CMD_ERROR, "UNKNOWN_CMD");
                send_packet(sock, &resp);
                log_message("WARN", "Unknown command");
                break;
```

**Unimplemented/unknown**:
- Send error responses
- Warn in logs

---

### Cleanup

```c
end_loop:
    close(sock);
    free(ctx);
    log_message("INFO", "Client thread exiting");
    return NULL;
}
```

**Thread exit**:
- Close the client socket (releases FD)
- Free the thread arguments structure
- Log exit
- Return NULL (required by pthread signature)

---

---

# File: `core/server/auth.c`

## Global State

```c
static UserRecord users[MAX_USERS];
static int users_count = 0;
static pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;
```

**Storage**:
- `users[]` = array of up to 15 user records (hardcoded limit)
- `users_count` = how many users currently loaded
- `users_mutex` = protects access to the array (prevents race conditions)

---

## Structure: `UserRecord`

```c
typedef struct {
    char username[USERNAME_LEN];   // 64 bytes
    char password[PASSWORD_LEN];   // 64 bytes
} UserRecord;
```

Simple username/password pair (plaintext, no hashing).

---

## Helper: `sanitize_field()`

```c
static void sanitize_field(char *s) {
    char *src = s, *dst = s;
    while (*src) {
        if (*src != '"' && *src != ' ' && *src != '\t' && *src != '\r' && *src != '\n')
            *dst++ = *src;
        src++;
    }
    *dst = '\0';
}
```

**Purpose**: Remove quotes and whitespace from parsed fields.

**Example**:
- Input: `"john"` (with quotes from JSON)
- Output: `john` (quotes removed)

**How it works**:
- `src` = pointer to source characters
- `dst` = pointer to destination (same buffer, but trailing)
- Copy `*src` to `*dst` only if NOT quote/space/tab/CR/LF
- Advance both pointers
- Null-terminate at the end

---

## Function: `load_users()`

```c
int load_users(void) {
    pthread_mutex_lock(&users_mutex);
```

**Lock the mutex**:
- Prevent other threads from modifying users array while we load
- Only one thread can hold the lock at a time

```c
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) {
        log_message("ERROR", "load_users: cannot open users.json");
        pthread_mutex_unlock(&users_mutex);
        return -1;
    }
```

**Open users file**:
- `USERS_FILE = "data/users.json"`
- Open in text read mode
- **Important**: Unlock before returning on error

```c
    char buf[512];
    int count = 0;
    while (fgets(buf, sizeof(buf), fp) && count < MAX_USERS) {
```

**Read file line by line**:
- `fgets()` = read one line (up to 512 chars) from file
- Loop while:
  - `fgets()` returns non-NULL (not EOF)
  - `count < MAX_USERS` (haven't loaded 15 users yet)

```c
        char *p = strchr(buf, '"');
        if (!p) continue;
        char *q = strchr(p + 1, '"');
        if (!q) continue;

        char username[USERNAME_LEN] = {0};
        size_t un_len = (size_t)(q - p - 1);
        if (un_len >= sizeof(username)) un_len = sizeof(username) - 1;
        strncpy(username, p + 1, un_len);
```

**Parse username**:
- Find first `"` in line (start of username)
- Find next `"` after that (end of username)
- If not found, skip this line
- Calculate length: `(q - p - 1)` = distance between quotes
- Copy to temporary buffer
- `strncpy()` = copy up to `un_len` characters (bounded to prevent overflow)

Example line: `"john":"password"`
```
p → "john
q → " (after john)
username = "john"
```

```c
        char *r = strchr(q + 1, '"');
        if (!r) continue;
        char *s = strchr(r + 1, '"');
        if (!s) continue;

        char password[PASSWORD_LEN] = {0};
        size_t pw_len = (size_t)(s - r - 1);
        if (pw_len >= sizeof(password)) pw_len = sizeof(password) - 1;
        strncpy(password, r + 1, pw_len);
```

**Parse password**:
- Same process for password (find quotes, extract text between them)

```c
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
```

**Store and finalize**:
- Remove quotes/whitespace from parsed fields
- Store in global users array
- Increment count
- After loop, close file
- Update global `users_count`
- Log how many users loaded
- Unlock mutex
- Return count

---

## Function: `authenticate_user()`

```c
int authenticate_user(const char *username, const char *password) {
    if (users_count == 0) {
        if (load_users() <= 0) {
            log_message("ERROR", "authenticate_user: no users loaded");
            return 0;
        }
    }
```

**Lazy load**:
- First time `authenticate_user()` is called, users array is empty
- Load from disk on first use
- If load fails, return 0 (auth failure)

```c
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
```

**Linear search**:
- Lock the mutex (prevent concurrent modification)
- Loop through all loaded users
- Compare both username AND password
- If both match, unlock and return 1 (authenticated)
- If we finish loop without match, unlock and return 0 (not authenticated)

---

---

# File: `core/server/file_ops.c`

## Helper: `ensure_base_dir()`

```c
static void ensure_base_dir(void) {
    struct stat st;
    if (stat(STORAGE_BASE, &st) == -1) {
        mkdir(STORAGE_BASE, 0755);
    }
}
```

**Purpose**: Create `data/storage/` directory if it doesn't exist.

**How it works**:
- `stat()` = get file info
- If returns -1, file/directory doesn't exist
- `mkdir()` with mode `0755` = rwxr-xr-x permissions
  - Owner: read, write, execute
  - Group: read, execute
  - Others: read, execute

---

## Helper: `ensure_user_dir()`

```c
static void ensure_user_dir(const char *user) {
    char path[PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", STORAGE_BASE, user);
    struct stat st;
    if (stat(path, &st) == -1) {
        ensure_base_dir();
        mkdir(path, 0755);
    }
}
```

**Purpose**: Create user directory like `data/storage/john/` if it doesn't exist.

**Process**:
- Build path: "data/storage/john"
- If directory doesn't exist:
  - Ensure base directory exists (recursive safety)
  - Create user directory

---

## Helper: `build_path()`

```c
static void build_path(char *dest, size_t len, const char *user, const char *filename) {
    snprintf(dest, len, "%s/%s/%s", STORAGE_BASE, user, filename);
}
```

**Purpose**: Construct full file path.

**Example**:
- Inputs: user="john", filename="file.txt"
- Output: "data/storage/john/file.txt"

---

## Function: `handle_file_upload()`

```c
int handle_file_upload(int sockfd, Packet *initial_request) {
    char user[USERNAME_LEN] = {0};
    char filename[FILE_NAME_LEN] = {0};
    size_t filesize = 0;

    if (sscanf(initial_request->data, "%63[^:]:%255[^:]:%zu", user, filename, &filesize) != 3) {
        log_message("ERROR", "handle_file_upload: bad header");
        return -1;
    }
```

**Parse upload header**:
- Data format: "username:filename:size"
- `%63[^:]` = username (up to 63 chars until ':')
- `%255[^:]` = filename (up to 255 chars until ':')
- `%zu` = filesize as unsigned integer (size_t)
- If parsing doesn't return 3 values, header is malformed

Example: "john:document.pdf:51200"

```c
    ensure_user_dir(user);

    char fullpath[PATH_LEN];
    build_path(fullpath, sizeof(fullpath), user, filename);

    FILE *fp = fopen(fullpath, "wb");
    if (!fp) {
        log_message("ERROR", "handle_file_upload: fopen failed");
        return -1;
    }
```

**Create file**:
- Create user directory if needed
- Build full path: "data/storage/john/document.pdf"
- Open file in binary write mode
- If fails (disk full, permissions), log error and return -1

```c
    size_t total = 0;
    char buf[CHUNK_SIZE];
    while (total < filesize) {
        ssize_t r = recv(sockfd, buf, CHUNK_SIZE, 0);
        if (r <= 0) {
            if (r == 0) log_message("WARN", "handle_file_upload: client closed");
            else log_message("ERROR", "handle_file_upload: recv error");
            fclose(fp);
            return -1;
        }
        fwrite(buf, 1, (size_t)r, fp);
        total += (size_t)r;
    }

    fclose(fp);
```

**Receive file data**:
- Loop until we've received all `filesize` bytes
- On each iteration:
  - `recv()` = read up to 4KB from socket into buffer
  - If <= 0, connection broke or client disconnected
  - Write buffer to file
  - Add bytes to total
- Close file when done

```c
    char msg[256];
    snprintf(msg, sizeof(msg), "Uploaded %s for %s (%zu bytes)", filename, user, total);
    log_message("INFO", msg);
    return 0;
}
```

**Finalize**:
- Log success with filename, user, and bytes received
- Return 0

---

## Function: `handle_file_download()`

```c
int handle_file_download(int sockfd, const char *data) {
    char user[USERNAME_LEN] = {0};
    char filename[FILE_NAME_LEN] = {0};

    if (sscanf(data, "%63[^:]:%255s", user, filename) != 2) {
        log_message("ERROR", "handle_file_download: bad request");
        return -1;
    }
```

**Parse download request**:
- Data format: "username:filename"
- Extract both fields
- If parsing fails, log error and return -1

```c
    char fullpath[PATH_LEN];
    build_path(fullpath, sizeof(fullpath), user, filename);

    FILE *fp = fopen(fullpath, "rb");
    if (!fp) {
        log_message("WARN", "handle_file_download: file not found");
        Packet err;
        init_packet(&err, CMD_ERROR, "FILE_NOT_FOUND");
        send_packet(sockfd, &err);
        return -1;
    }
```

**Open file**:
- Build path
- Open in binary read mode
- If not found:
  - Log warning (file not found is not critical)
  - Send error packet to client
  - Return -1

```c
    fseek(fp, 0, SEEK_END);
    size_t filesize = (size_t)ftell(fp);
    rewind(fp);

    char header[64];
    snprintf(header, sizeof(header), "%zu", filesize);
    Packet ack;
    init_packet(&ack, CMD_ACK, header);
    if (send_packet(sockfd, &ack) < 0) {
        fclose(fp);
        log_message("ERROR", "handle_file_download: send_packet failed");
        return -1;
    }
```

**Send file size**:
- Get file size by seeking to end
- Create ACK packet with filesize as payload
- Send to client
- Client uses this to know how many bytes to expect
- On send failure, close file and return -1

```c
    char buf[CHUNK_SIZE];
    size_t sent = 0;
    size_t n;
    while ((n = fread(buf, 1, CHUNK_SIZE, fp)) > 0) {
        if (send_all(sockfd, buf, n) < 0) {
            fclose(fp);
            log_message("ERROR", "handle_file_download: send_all failed");
            return -1;
        }
        sent += n;
    }

    fclose(fp);
    char msg[256];
    snprintf(msg, sizeof(msg), "Sent %s to client (%zu bytes)", filename, sent);
    log_message("INFO", msg);
    return 0;
}
```

**Stream file data**:
- Loop: read up to 4KB from file into buffer
  - `fread()` returns number of bytes actually read
  - Returns 0 when EOF reached
- For each chunk:
  - `send_all()` = send entire buffer over socket (retries on partial sends)
  - If fails, close file and return -1
  - Add bytes to total
- After sending all data, close file
- Log completion with filename and bytes sent
- Return 0 (success)

---

## Function: `cleanup_user_data()`

```c
void cleanup_user_data() {
    const char *storage_dir = "data/storage";
    const char *user_file = "data/users.json";

    DIR *dir = opendir(storage_dir);
    if (dir) {
        struct dirent *entry;
        char path[PATH_MAX];

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            snprintf(path, sizeof(path), "%s/%s", storage_dir, entry->d_name);

            char cmd[PATH_MAX * 2];
            snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
            system(cmd);

            char msg[PATH_MAX + 64];
            snprintf(msg, sizeof(msg), "Deleted user directory: %s", path);
            log_message("INFO", msg);
        }
        closedir(dir);
    }

    if (remove(user_file) == 0) {
        log_message("INFO", "Removed users.json (user profiles cleared).");
    } else {
        log_message("WARN", "Could not remove users.json (missing or locked).");
    }

    log_message("INFO", "All user data has been cleared successfully.");
}
```

**Purpose**: Delete all user data when server shuts down.

**Process**:
- `opendir()` = open directory handle for `data/storage/`
- Loop through all entries:
  - Skip `.` and `..` (special directory pointers)
  - Build path to entry
  - Use `system()` to execute shell command: `rm -rf "path"`
    - **⚠️ SECURITY RISK**: If filenames contain shell metacharacters, code injection possible
  - Log deletion
- Close directory handle
- Delete `data/users.json` file
- Log final cleanup message

**⚠️ Critical Issues**:
1. Using `system("rm -rf")` is dangerous and non-portable
2. No path traversal protection (could delete `../../../etc/` if not careful)
3. Relies on shell being available

---

---

# File: `core/common/protocol.c`

These functions handle serializing/deserializing the custom binary protocol.

## Function: `init_packet()`

```c
void init_packet(Packet *pkt, CommandType cmd, const char *data) {
    memset(pkt, 0, sizeof(Packet));
    pkt->command = (uint32_t)cmd;
    if (data) {
        size_t len = strlen(data);
        if (len > MAX_PAYLOAD) len = MAX_PAYLOAD;
        memcpy(pkt->data, data, len);
        pkt->data_length = (uint32_t)len;
    } else {
        pkt->data_length = 0;
    }
}
```

**Purpose**: Create a packet in memory.

**Process**:
- Zero-out entire packet
- Set command type
- If data provided:
  - Get string length
  - Cap at MAX_PAYLOAD (4096 bytes) to prevent overflow
  - Copy data to packet buffer
  - Set data_length field
- Otherwise, set data_length to 0

**Example**:
```c
Packet p;
init_packet(&p, CMD_AUTH, "john:password123");
// p.command = CMD_AUTH
// p.data = "john:password123"
// p.data_length = 17
```

---

## Function: `send_packet()`

```c
int send_packet(int sockfd, const Packet *pkt) {
    uint32_t net_cmd = htonl(pkt->command);
    uint32_t net_len = htonl(pkt->data_length);

    /* send header */
    if (send_all(sockfd, &net_cmd, sizeof(net_cmd)) < 0) return -1;
    if (send_all(sockfd, &net_len, sizeof(net_len)) < 0) return -1;

    /* send payload if any */
    if (pkt->data_length > 0) {
        if (send_all(sockfd, pkt->data, pkt->data_length) < 0) return -1;
    }
    return 0;
}
```

**Purpose**: Convert packet to network format and transmit.

**Wire format**:
```
[4 bytes: command (big-endian)]
[4 bytes: data_length (big-endian)]
[0-4096 bytes: data payload]
```

**Process**:
- `htonl()` = "host to network long" (convert to big-endian)
- Send 4-byte command in network order
- Send 4-byte length in network order
- If length > 0, send payload
- Return -1 if any send fails, 0 on success

**Example transmission**:
```
Packet with CMD_AUTH and data "john:password"

On wire:
00 00 00 01           (command = 1, CMD_AUTH, big-endian)
00 00 00 0D           (length = 13 bytes, big-endian)
6A 6F 68 6E 3A 70 61 73 73 77 6F 72 64  (ASCII "john:password")
```

---

## Function: `recv_packet()`

```c
int recv_packet(int sockfd, Packet *pkt) {
    uint32_t net_cmd;
    uint32_t net_len;

    if (recv_all(sockfd, &net_cmd, sizeof(net_cmd)) <= 0) return -1;
    if (recv_all(sockfd, &net_len, sizeof(net_len)) <= 0) return -1;

    pkt->command = ntohl(net_cmd);
    pkt->data_length = ntohl(net_len);

    if (pkt->data_length > 0) {
        if (pkt->data_length > MAX_PAYLOAD) return -1;
        if (recv_all(sockfd, pkt->data, pkt->data_length) <= 0) return -1;
    }
    return 0;
}
```

**Purpose**: Receive and deserialize a packet from the socket.

**Process**:
- Receive 4-byte command
- Receive 4-byte length
- `ntohl()` = "network to host long" (convert from big-endian)
- If length > 0:
  - Validate length doesn't exceed MAX_PAYLOAD (prevent buffer overflow)
  - Receive payload bytes
- Return 0 on success, -1 on failure

---

## Function: `command_to_string()`

```c
const char *command_to_string(uint32_t cmd) {
    switch (cmd) {
        case CMD_AUTH: return "AUTH";
        case CMD_UPLOAD: return "UPLOAD";
        case CMD_DOWNLOAD: return "DOWNLOAD";
        case CMD_LIST: return "LIST";
        case CMD_DELETE: return "DELETE";
        case CMD_EXIT: return "EXIT";
        case CMD_ACK: return "ACK";
        case CMD_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
```

**Purpose**: Convert command enum to human-readable string for logging.

---

---

# File: `core/common/common.c`

## Global State

```c
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
```

Protects logging system from concurrent writes (multiple threads logging simultaneously).

---

## Function: `ensure_log_dir()`

```c
static void ensure_log_dir(void) {
    struct stat st;
    if (stat(LOG_DIR, &st) == -1) {
        mkdir(LOG_DIR, 0755);
    }
}
```

**Purpose**: Create `data/logs/` directory if it doesn't exist.

---

## Function: `get_timestamp()`

```c
void get_timestamp(char *buffer, size_t len) {
    time_t now = time(NULL);
    struct tm tbuf;
    localtime_r(&now, &tbuf);
    strftime(buffer, len, "%Y-%m-%d %H:%M:%S", &tbuf);
}
```

**Purpose**: Get current time as formatted string.

**Process**:
- `time()` = get current Unix timestamp
- `localtime_r()` = convert to broken-down time (thread-safe version with `_r`)
- `strftime()` = format as "2025-11-18 14:30:45"

**Example output**:
```
"2025-11-18 14:30:45"
```

---

## Function: `get_log_filename()`

```c
void get_log_filename(char *buffer, size_t len) {
    time_t now = time(NULL);
    struct tm tbuf;
    localtime_r(&now, &tbuf);
    snprintf(buffer, len, "%s/%s-%04d-%02d-%02d.log",
             LOG_DIR, LOG_FILE_BASE,
             tbuf.tm_year + 1900, tbuf.tm_mon + 1, tbuf.tm_mday);
}
```

**Purpose**: Generate log filename for today.

**Process**:
- Get current time
- Format as: "data/logs/server-YYYY-MM-DD.log"
- Note: `tm_year` is years since 1900, so add 1900
- Note: `tm_mon` is 0-11, so add 1 for 1-12

**Example**:
```
"data/logs/server-2025-11-18.log"
```

---

## Function: `init_logging()`

```c
void init_logging(void) {
    ensure_log_dir();
    log_message("INFO", "Logging initialized");
}
```

**Purpose**: Initialize logging system on startup.

---

## Function: `log_message()`

```c
void log_message(const char *level, const char *message) {
    pthread_mutex_lock(&log_mutex);

    ensure_log_dir();

    char filename[PATH_LEN];
    get_log_filename(filename, sizeof(filename));

    FILE *fp = fopen(filename, "a");
    if (!fp) {
        /* If we can't open the file, at least print to stderr */
        fprintf(stderr, "log_message: fopen failed: %s\n", strerror(errno));
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    char ts[64];
    get_timestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [%s] %s\n", ts, level, message);
    fclose(fp);

    pthread_mutex_unlock(&log_mutex);
}
```

**Purpose**: Thread-safe logging to daily log files.

**Process**:
- Lock mutex (prevent concurrent writes)
- Ensure logs directory exists
- Get today's log filename
- Open in append mode
- If open fails, print error to stderr and return
- Get current timestamp
- Write formatted line: `[2025-11-18 14:30:45] [INFO] User john authenticated`
- Close file
- Unlock mutex

**Example log entry**:
```
[2025-11-18 14:30:45] [INFO] User john authenticated
[2025-11-18 14:30:46] [INFO] Uploaded file.txt for john (1024 bytes)
[2025-11-18 14:30:47] [WARN] Authentication failed
```

---

## Function: `handle_error()`

```c
void handle_error(const char *msg) {
    perror(msg);
    log_message("ERROR", msg);
    exit(EXIT_FAILURE);
}
```

**Purpose**: Log error and exit program.

**Process**:
- `perror()` = print error to stderr with system error message
- Log to file
- Exit with failure code

**Example**:
```c
if (bind(sock, ...) < 0) {
    handle_error("bind");
}
// Output: "bind: Address already in use"
```

---

## Function: `send_all()`

```c
ssize_t send_all(int sockfd, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = (const char*)buf;
    while (total < len) {
        ssize_t n = send(sockfd, p + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    return (ssize_t)total;
}
```

**Purpose**: Send all bytes over socket, handling partial sends.

**Why needed**:
- Single `send()` call might not send all requested bytes
- Kernel buffer might fill up
- Need to loop and retry

**Process**:
- Track total bytes sent
- Loop while haven't sent everything
- Call `send()` for remaining bytes
- If returns < 0 (error):
  - If EINTR (interrupted by signal), retry
  - Otherwise return -1 (real error)
- If returns 0, connection closed
- Add bytes to total
- Return total bytes sent

**Example**:
```c
char data[8192];
send_all(sockfd, data, 8192);
// Might take 3 send() calls:
// - send() returns 4096
// - send() returns 2048
// - send() returns 2048
// Total: 8192
```

---

## Function: `recv_all()`

```c
ssize_t recv_all(int sockfd, void *buf, size_t len) {
    size_t total = 0;
    char *p = (char*)buf;
    while (total < len) {
        ssize_t n = recv(sockfd, p + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    return (ssize_t)total;
}
```

**Purpose**: Receive all bytes from socket, handling partial receives.

**Similar to `send_all()`**:
- Loop until received all requested bytes
- Handle EINTR (retry on signal)
- Return total bytes received

---

---

# Architecture Diagram

```
┌──────────────────────────────────────────────────────────┐
│                   LocalBin Server                        │
│                  (Multi-threaded TCP)                    │
└──────────────────────────────────────────────────────────┘
                          │
                    main.c:main()
                          │
                          ▼
        ┌─────────────────────────────────┐
        │  Register SIGINT/SIGTERM        │
        │  Initialize logging             │
        │  Call start_server(port)        │
        └─────────────────────────────────┘
                          │
                          ▼
        ┌─────────────────────────────────┐
        │    server.c:start_server()      │
        ├─────────────────────────────────┤
        │ 1. Create listening socket      │
        │ 2. Bind to port                 │
        │ 3. Listen for connections       │
        │ 4. Main accept loop:            │
        │    while(server_running) {      │
        │      accept() <- BLOCKING       │
        │      spawn thread for client    │
        │    }                            │
        │ 5. Cleanup and shutdown         │
        └─────────────────────────────────┘
                          │
            ┌─────────────┴─────────────┐
            │ For each client...        │
            ▼                           ▼
    ┌──────────────────────┐    ┌──────────────────────┐
    │  Thread 1            │    │  Thread N            │
    │  (Client 1)          │    │  (Client N)          │
    │  FD=5                │    │  FD=X                │
    └──────────────────────┘    └──────────────────────┘
            │                           │
            ▼                           ▼
    client_handler.c:          client_handler.c:
    client_thread()            client_thread()
            │                           │
            ├─────────────┬─────────────┤
            │             │             │
            ▼             ▼             ▼
        CMD_AUTH      CMD_UPLOAD    CMD_DOWNLOAD
            │             │             │
            ▼             ▼             ▼
    auth.c:          file_ops.c:    file_ops.c:
    authenticate_user() handle_upload() handle_download()
            │             │             │
            ├─────────────┴─────────────┤
            │                           │
            ▼                           ▼
    common.c:log_message()      protocol.c:send_packet()
    (Thread-safe logging)       protocol.c:recv_packet()
                                (Serialize/deserialize)
```

---

# Data Flow: Upload Example

```
Client connects to port 8080
    │
    ▼
main.c: accept()
    │
    ├─ Creates new thread
    ├─ Passes socket FD to thread
    │
    ▼
client_thread(arg) starts
    │
    ├─ authenticated = 0
    │
    ▼
recv_packet() ← AUTH: "john:password"
    │
    ▼
client_handler.c: CMD_AUTH case
    │
    ├─ sscanf() parse credentials
    │
    ▼
auth.c: authenticate_user("john", "password")
    │
    ├─ Lock users_mutex
    ├─ Search users[] array
    ├─ Compare username & password
    │
    ▼
    ├─ Match found!
    │
    ├─ authenticated = 1
    ├─ current_user = "john"
    │
    ├─ send_packet(CMD_ACK, "AUTH_OK")
    │
    ▼
recv_packet() ← UPLOAD: "john:document.pdf:51200"
    │
    ▼
client_handler.c: CMD_UPLOAD case
    │
    ├─ if (!authenticated) reject
    │
    ▼
file_ops.c: handle_file_upload()
    │
    ├─ sscanf() parse: user, filename, filesize
    ├─ ensure_user_dir("john")
    │   └─ mkdir("data/storage/john")
    │
    ├─ fopen("data/storage/john/document.pdf", "wb")
    │
    ├─ Loop: recv() 4KB chunks
    │   ├─ Chunk 1: recv() 4096 bytes
    │   ├─ fwrite() to file
    │   ├─ Chunk 2-12: repeat
    │   │
    │   └─ Total: 51200 bytes received
    │
    ├─ fclose()
    │
    ├─ common.c: log_message("INFO", "Uploaded ...")
    │
    ▼
send_packet(CMD_ACK, "UPLOAD_OK")
    │
    └─ Back to recv_packet() waiting for next command
```

---

# Thread Safety

## Protected by Mutex

**users_mutex** protects:
- `users[]` array
- `users_count` variable

When a thread calls `authenticate_user()`:
```c
pthread_mutex_lock(&users_mutex);
// Only this thread can access users[] now
for (int i = 0; i < users_count; ++i) {
    // Search for match
}
pthread_mutex_unlock(&users_mutex);
// Other threads can now access users[]
```

## Protected by Socket (No Explicit Lock)

**Per-client sockets** are isolated:
- Thread 1 uses FD 5 (only writes to FD 5)
- Thread 2 uses FD 6 (only writes to FD 6)
- No conflict because OS file descriptor table is per-process
- Kernel handles actual socket buffering

## Protected by Mutex

**log_mutex** protects:
- File writes to `data/logs/server-YYYY-MM-DD.log`
- Prevents garbled log lines when multiple threads log simultaneously

---

# Error Scenarios

## Scenario 1: Client Disconnects Mid-Upload

```
recv_packet() → returns -1
    │
    ▼
"client_thread: recv_packet failed or client disconnected"
    │
    ▼
break out of main loop
    │
    ▼
close(sock)
free(args)
thread exits
```

File might be incomplete but stored (no rollback mechanism).

## Scenario 2: Authentication Fails

```
Client sends: CMD_AUTH with wrong password
    │
    ▼
authenticate_user() returns 0
    │
    ▼
send_packet(CMD_ERROR, "AUTH_FAIL")
    │
    ▼
authenticated = 0 (unchanged)
    │
    ▼
Client tries to upload
    │
    ▼
if (!authenticated) → send error
    │
    └─ Client cannot access files
```

## Scenario 3: Disk Full During Download

```
handle_file_download()
    │
    ├─ fopen(fullpath, "wb")
    │
    ├─ send_packet(CMD_ACK, filesize) ✓
    │
    ├─ Loop: fread() then send_all()
    │   │
    │   └─ In the middle of sending...
    │       Network error or client closes
    │
    ├─ recv() returns <= 0
    │
    ▼
return -1
    │
    ▼
send_packet(CMD_ERROR, "DOWNLOAD_FAIL")
```

---

# Security Issues Summary

| Issue | Severity | Details |
|-------|----------|---------|
| Plaintext credentials in JSON | 🔴 CRITICAL | No password hashing |
| Plaintext network transmission | 🔴 CRITICAL | No TLS/encryption |
| Path traversal in filenames | 🔴 CRITICAL | `../../../etc/passwd` possible |
| Cross-user file access | 🔴 CRITICAL | No access control between users |
| Use of `system()` command | 🔴 CRITICAL | Command injection if filenames controlled |
| Hardcoded 15-user limit | 🟠 HIGH | Won't scale |
| No rate limiting | 🟠 HIGH | Brute force attacks possible |
| Buffer size limits | 🟠 HIGH | DoS if exceeded |
| Fixed 10-sec timeout | 🟡 MEDIUM | Might be too short for large files |


```
