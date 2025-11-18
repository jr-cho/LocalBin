# client.c - Detailed Line-by-Line Explanation

## File Overview
This file implements the **client-side** of the LocalBin file sharing system. It provides functions to connect to the server, authenticate, upload files, download files, and disconnect.

---

## Includes & Setup

```c
#include "client.h"
#include <arpa/inet.h>      // IP address conversion (inet_pton, inet_ntop)
#include <fcntl.h>          // File control options (not actively used here)
#include <netdb.h>          // Hostname resolution (getaddrinfo)
#include <string.h>         // String/memory operations (memset, strchr)
#include <netinet/tcp.h>    // TCP socket options (TCP_NODELAY)
```

These headers provide socket and networking utilities.

---

## Function 1: `client_connect()`

### Purpose
Establish a TCP connection to the server. Handles both IP addresses and hostnames.

```c
int client_connect(Client *c, const char *host, int port) {
    if (!c) return -1;
```
**Null check**: Ensure the Client pointer is valid. Return -1 on error.

```c
    memset(c, 0, sizeof(Client));
```
**Initialize**: Zero-out the entire Client struct to clear any garbage data.
- Sets `sockfd = 0`
- Sets `is_connected = 0`
- Clears the `server_addr` structure

```c
    c->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (c->sockfd < 0) {
        log_message("ERROR", "client_connect: socket creation failed");
        return -1;
    }
```
**Create socket**:
- `AF_INET` = IPv4 protocol family
- `SOCK_STREAM` = TCP (reliable, ordered delivery)
- `0` = use default protocol (IPPROTO_TCP)
- `socket()` returns a file descriptor (an integer)
- If FD is negative, socket creation failed (out of file descriptors, permissions issue, etc.)

```c
    int flag = 1;
    if (setsockopt(c->sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        log_message("WARN", "client_connect: could not set TCP_NODELAY");
    }
```
**Disable Nagle's Algorithm**:
- By default, TCP waits a bit to batch small packets (improves bandwidth but adds latency)
- `TCP_NODELAY` tells the kernel: send packets immediately, don't wait
- **Why**: For interactive apps (GUI), we want responsiveness over bandwidth efficiency
- Failure is non-critical (warning only), so we continue

```c
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    if (setsockopt(c->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message("WARN", "client_connect: could not set SO_RCVTIMEO");
    }
    if (setsockopt(c->sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_message("WARN", "client_connect: could not set SO_SNDTIMEO");
    }
```
**Set socket timeouts**:
- `SO_RCVTIMEO` = receive timeout (10 seconds)
- `SO_SNDTIMEO` = send timeout (10 seconds)
- **Why**: If the server hangs or network is dead, don't block forever
- If 10 seconds pass with no activity, `recv()`/`send()` return an error
- Both are non-critical warnings

```c
    int keepalive = 1;
    if (setsockopt(c->sockfd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        log_message("WARN", "client_connect: could not set SO_KEEPALIVE");
    }
```
**Enable TCP keepalive**:
- Periodically sends small probe packets to detect dead connections
- **Why**: If the network goes silent, detect it rather than hanging indefinitely
- Non-critical warning

```c
    c->server_addr.sin_family = AF_INET;
    c->server_addr.sin_port = htons(port);
```
**Set up address structure**:
- `sin_family` = IPv4
- `sin_port` = convert port to network byte order
  - `htons()` = "host to network short" (convert integer port number to network format)
  - Example: `8080` → `0x901f` (big-endian)

---

### Address Resolution (Two Paths)

```c
    if (inet_pton(AF_INET, host, &c->server_addr.sin_addr) <= 0) {
```
**Try direct IP parsing**:
- `inet_pton()` = "IP string to network" (parse "127.0.0.1" → binary form)
- Returns > 0 if successful, 0 if not a valid IP format, < 0 on error
- **If this succeeds**: Jump to connection attempt (IP address was provided)
- **If this fails**: Continue to hostname resolution (assumes it's a hostname like "example.com")

```c
        // If that fails, try hostname resolution
        struct addrinfo hints, *result, *rp;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
```
**Set up hostname resolution hints**:
- Tell `getaddrinfo()` what kind of results we want
- `AF_INET` = IPv4 only
- `SOCK_STREAM` = TCP
- `IPPROTO_TCP` = TCP protocol

```c
        char msg[256];
        snprintf(msg, sizeof(msg), "Attempting to resolve hostname: %s", host);
        log_message("INFO", msg);

        int res = getaddrinfo(host, NULL, &hints, &result);
        if (res != 0) {
            snprintf(msg, sizeof(msg), "client_connect: hostname resolution failed: %s", gai_strerror(res));
            log_message("ERROR", msg);
            close(c->sockfd);
            return -1;
        }
```
**Perform DNS lookup**:
- `getaddrinfo()` = convert hostname to IP address
  - Input: "google.com"
  - Output: linked list of possible IP addresses
- If it fails, close the socket and return -1
- Log the error using `gai_strerror()` to get human-readable message

```c
        int connected = 0;
        for (rp = result; rp != NULL; rp = rp->ai_next) {
            struct sockaddr_in *addr = (struct sockaddr_in *)rp->ai_addr;
            c->server_addr.sin_addr = addr->sin_addr;

            char resolved_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &c->server_addr.sin_addr, resolved_ip, INET_ADDRSTRLEN);
            snprintf(msg, sizeof(msg), "Resolved %s to %s, attempting connection...", host, resolved_ip);
            log_message("INFO", msg);

            if (connect(c->sockfd, (struct sockaddr*)&c->server_addr, sizeof(c->server_addr)) == 0) {
                connected = 1;
                break;
            }
        }
        freeaddrinfo(result);

        if (!connected) {
            snprintf(msg, sizeof(msg), "client_connect: could not connect to any resolved address for %s", host);
            log_message("ERROR", msg);
            close(c->sockfd);
            return -1;
        }
```
**Try each resolved address**:
- `getaddrinfo()` might return multiple IPs (for redundancy)
- Loop through each one and try to connect
- `inet_ntop()` = convert binary IP back to string for logging ("network to presentation")
- `connect()` returns 0 on success, -1 on failure
- If any succeeds, set `connected = 1` and break
- If all fail, clean up and return -1

---

### Direct IP Connection Path

```c
    } else {
        // Direct IP address connection
        char connect_msg[256];
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &c->server_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        snprintf(connect_msg, sizeof(connect_msg), 
                 "Attempting connection to %s:%d", ip_str, port);
        log_message("INFO", connect_msg);

        if (connect(c->sockfd, (struct sockaddr*)&c->server_addr, 
                    sizeof(c->server_addr)) < 0) {
            snprintf(connect_msg, sizeof(connect_msg), 
                     "client_connect: connect() failed to %s:%d - %s", 
                     ip_str, port, strerror(errno));
            log_message("ERROR", connect_msg);
            close(c->sockfd);
            return -1;
        }
    }
```
**Connect using the direct IP**:
- If we already had a valid IP (the first `inet_pton()` succeeded), use it directly
- `connect()` = initiate TCP handshake to the server
- `sizeof(c->server_addr)` tells the kernel how big the address structure is
- On failure, log the error with `strerror(errno)` (kernel error message)
- Close socket and return -1

---

### Success Path

```c
    c->is_connected = 1;
    
    char success_msg[256];
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &c->server_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    snprintf(success_msg, sizeof(success_msg), 
             "Client successfully connected to %s:%d", ip_str, port);
    log_message("INFO", success_msg);
    
    return 0;
```
**Connection succeeded**:
- Set `is_connected = 1` flag
- Log success message with the IP and port
- Return 0 (success)

---

## Function 2: `client_auth()`

### Purpose
Authenticate with the server using username and password.

```c
int client_auth(Client *c, const char *username, const char *password) {
    if (!c || !c->is_connected) {
        log_message("ERROR", "client_auth: not connected");
        return -1;
    }
```
**Precondition check**:
- Ensure Client pointer is valid
- Ensure we've already connected (`is_connected == 1`)

```c
    char data[USERNAME_LEN + PASSWORD_LEN + 8];
    snprintf(data, sizeof(data), "%s:%s", username, password);
```
**Format credentials**:
- Create a buffer large enough for "username:password"
- Use colon as delimiter (e.g., "john:secret123")

```c
    Packet p;
    init_packet(&p, CMD_AUTH, data);
    if (send_packet(c->sockfd, &p) < 0) {
        log_message("ERROR", "client_auth: send_packet failed");
        return -1;
    }
```
**Send AUTH packet**:
- `init_packet()` = create a packet with command `CMD_AUTH` and the credentials as payload
- `send_packet()` = serialize to network format and transmit over socket
- Returns -1 on failure

```c
    Packet resp;
    if (recv_packet(c->sockfd, &resp) < 0) {
        log_message("ERROR", "client_auth: recv_packet failed");
        return -1;
    }
```
**Wait for response**:
- Block until server sends a response packet
- Returns -1 if receive fails

```c
    if (resp.command == CMD_ACK && strstr(resp.data, "AUTH_OK")) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Authentication successful for user: %s", username);
        log_message("INFO", msg);
        return 0;
    }

    log_message("WARN", "Authentication failed - invalid credentials");
    return -1;
```
**Check response**:
- Verify server sent `CMD_ACK` (acknowledgment) with "AUTH_OK" in the payload
- If yes: log success and return 0
- If no: log warning and return -1

---

## Function 3: `client_upload()`

### Purpose
Upload a file from the local filesystem to the server.

```c
int client_upload(Client *c, const char *username, const char *filepath) {
    if (!c || !c->is_connected) {
        log_message("ERROR", "client_upload: not connected");
        return -1;
    }
```
**Precondition check**: Ensure connected.

```c
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        char msg[256];
        snprintf(msg, sizeof(msg), "client_upload: cannot open file: %s", filepath);
        log_message("ERROR", msg);
        return -1;
    }
```
**Open local file**:
- `fopen(..., "rb")` = open file in binary read mode
- If NULL, file doesn't exist or no permission
- Log error and return -1

```c
    fseek(fp, 0, SEEK_END);
    size_t filesize = (size_t)ftell(fp);
    rewind(fp);
```
**Get file size**:
- `fseek()` to end of file
- `ftell()` returns current position = file size in bytes
- `rewind()` go back to start for reading

```c
    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;
```
**Extract just the filename**:
- `strrchr()` = find last occurrence of '/' (directory separator)
- If found: use everything after the '/' (filename only)
- If not found: use the whole path (already just a filename)
- Example: "/home/user/file.txt" → "file.txt"

```c
    char header[USERNAME_LEN + FILE_NAME_LEN + 64];
    snprintf(header, sizeof(header), "%s:%s:%zu", username, filename, filesize);
```
**Create upload header**:
- Format: "username:filename:size"
- Example: "john:file.txt:1024"

```c
    Packet p;
    init_packet(&p, CMD_UPLOAD, header);
    if (send_packet(c->sockfd, &p) < 0) {
        log_message("ERROR", "client_upload: send_packet failed for header");
        fclose(fp);
        return -1;
    }
```
**Send upload header packet**:
- Server uses this to create the file and prepare for data
- **Important**: Close file before returning on error

```c
    char buffer[BUFFER_SIZE];
    size_t n;
    size_t sent = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        ssize_t result = send_all(c->sockfd, buffer, n);
        if (result < 0 || (size_t)result != n) {
            char msg[128];
            snprintf(msg, sizeof(msg), "client_upload: send_all failed (sent %zu/%zu)", sent, filesize);
            log_message("ERROR", msg);
            fclose(fp);
            return -1;
        }
        sent += n;
    }
    fclose(fp);
```
**Stream file data**:
- `fread()` = read up to 4KB from file into buffer
  - Returns number of bytes actually read
  - Returns 0 when EOF reached
- Loop while `n > 0` (data available)
- `send_all()` = send buffer over socket, retrying on partial sends
  - Returns actual bytes sent
  - Must equal `n`, otherwise network error
- Track total bytes sent for logging
- Close file when done (or on error)

```c
    char msg[256];
    snprintf(msg, sizeof(msg), "client_upload: sent %zu bytes, waiting for ACK", sent);
    log_message("INFO", msg);

    Packet resp;
    if (recv_packet(c->sockfd, &resp) == 0 && resp.command == CMD_ACK) {
        snprintf(msg, sizeof(msg), "Upload completed successfully: %s (%zu bytes)", filename, sent);
        log_message("INFO", msg);
        return 0;
    }
    
    log_message("WARN", "Upload failed or not acknowledged by server");
    return -1;
```
**Wait for server ACK**:
- Server should send back an acknowledgment packet
- If received successfully (`recv_packet() == 0`) AND it's a `CMD_ACK`:
  - Log success and return 0
- Otherwise, log warning and return -1

---

## Function 4: `client_download()`

### Purpose
Download a file from the server to the local filesystem.

```c
int client_download(Client *c, const char *username, const char *filename, const char *save_path) {
    if (!c || !c->is_connected) {
        log_message("ERROR", "client_download: not connected");
        return -1;
    }
```
**Precondition check**: Ensure connected.

```c
    char header[USERNAME_LEN + FILE_NAME_LEN + 8];
    snprintf(header, sizeof(header), "%s:%s", username, filename);

    Packet req;
    init_packet(&req, CMD_DOWNLOAD, header);
    if (send_packet(c->sockfd, &req) < 0) {
        log_message("ERROR", "client_download: send_packet failed");
        return -1;
    }
```
**Send download request**:
- Format: "username:filename"
- Server will look up the file in its storage

```c
    Packet ack;
    if (recv_packet(c->sockfd, &ack) < 0) {
        log_message("ERROR", "client_download: recv_packet failed");
        return -1;
    }
    
    if (ack.command != CMD_ACK) {
        char msg[256];
        snprintf(msg, sizeof(msg), "client_download: server error: %s", ack.data);
        log_message("ERROR", msg);
        return -1;
    }
```
**Get server response**:
- Server should send back `CMD_ACK` with the file size as payload
- If server sends `CMD_ERROR` instead, extract error message and return -1

```c
    size_t filesize = strtoull(ack.data, NULL, 10);
    if (filesize == 0) {
        log_message("WARN", "client_download: empty file or parse error");
        return -1;
    }
```
**Parse file size**:
- `strtoull()` = convert string to unsigned long long (in base 10)
- Example: "1024" → 1024 bytes
- If size is 0, either file is empty or parsing failed

```c
    char fullpath[PATH_LEN];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", save_path, filename);
    FILE *fp = fopen(fullpath, "wb");
    if (!fp) {
        char msg[256];
        snprintf(msg, sizeof(msg), "client_download: cannot open save path: %s", fullpath);
        log_message("ERROR", msg);
        return -1;
    }
```
**Create output file**:
- Combine save directory path with filename
- Example: "/home/user" + "file.txt" → "/home/user/file.txt"
- Open in binary write mode
- Log error if can't create (permissions, disk full, etc.)

```c
    char buffer[BUFFER_SIZE];
    size_t total = 0;
    ssize_t r;
    
    char msg[256];
    snprintf(msg, sizeof(msg), "client_download: receiving %zu bytes", filesize);
    log_message("INFO", msg);
    
    while (total < filesize) {
        size_t to_read = (filesize - total < BUFFER_SIZE) ? (filesize - total) : BUFFER_SIZE;
        r = recv(c->sockfd, buffer, to_read, 0);
        if (r <= 0) {
            snprintf(msg, sizeof(msg), "client_download: recv failed after %zu bytes", total);
            log_message("ERROR", msg);
            fclose(fp);
            return -1;
        }
        fwrite(buffer, 1, (size_t)r, fp);
        total += (size_t)r;
    }
    fclose(fp);
```
**Receive file data**:
- Loop until we've received `filesize` bytes
- On each iteration:
  - Calculate how much to read (min of remaining bytes or 4KB buffer)
  - `recv()` = read from socket into buffer
    - Returns number of bytes received
    - Returns ≤ 0 on error/disconnect
  - Write buffer to file using `fwrite()`
  - Add bytes to total
- If `recv()` returns ≤ 0, connection broke or error occurred
- Close file when done

```c
    snprintf(msg, sizeof(msg), "File download complete: %s (%zu bytes)", filename, total);
    log_message("INFO", msg);
    return 0;
```
**Success**:
- Log completion and return 0

---

## Function 5: `client_disconnect()`

### Purpose
Gracefully close the connection and clean up.

```c
void client_disconnect(Client *c) {
    if (!c || !c->is_connected) return;
```
**Precondition check**: Only proceed if connected.

```c
    Packet p;
    init_packet(&p, CMD_EXIT, "EXIT");
    send_packet(c->sockfd, &p);
```
**Send EXIT packet**:
- Tell server we're leaving
- Don't check for errors (connection might already be broken)

```c
    close(c->sockfd);
    c->is_connected = 0;
    log_message("INFO", "Client disconnected gracefully");
}
```
**Close socket**:
- `close()` = release the file descriptor and close TCP connection
- Set flag to indicate we're no longer connected
- Log the event

---

## Data Flow Summary

```
┌─────────────────────────────────────────────────┐
│  User calls functions from Python GUI           │
└──────────────┬──────────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────────┐
│  client_connect()                               │
│  ├─ Create TCP socket                           │
│  ├─ Set socket options (TCP_NODELAY, timeouts)  │
│  ├─ Parse hostname or IP                        │
│  └─ Establish TCP connection                    │
└──────────────┬──────────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────────┐
│  client_auth()                                  │
│  ├─ Send "username:password" packet             │
│  └─ Wait for CMD_ACK with "AUTH_OK"             │
└──────────────┬──────────────────────────────────┘
               │
        ┌──────┴───────┐
        │              │
        ▼              ▼
┌────────────────┐  ┌─────────────────┐
│ client_upload()│  │client_download()│
│                │  │                 │
│ 1. Open file   │  │ 1. Send request │
│ 2. Get size    │  │ 2. Get filesize │
│ 3. Send header │  │ 3. Create file  │
│ 4. Stream data │  │ 4. Stream data  │
│ 5. Get ACK     │  │ 5. Close file   │
└────────────────┘  └─────────────────┘
        │              │
        └──────┬───────┘
               │
               ▼
┌─────────────────────────────────────────────────┐
│  client_disconnect()                            │
│  ├─ Send EXIT packet                            │
│  └─ Close socket                                │
└─────────────────────────────────────────────────┘
```

---

## Key Design Patterns

### Error Handling
- All functions return 0 on success, -1 on failure
- Errors are logged with context (what operation, what failed)

### Resource Management
- Always `close()` socket on error
- Always `fclose()` file before returning
- Timeouts prevent indefinite blocking

### Protocol
- Send header packet first (metadata)
- Then stream data
- Wait for ACK confirmation
- Use colon-delimited format: "field1:field2:field3"

### Logging
- INFO: successful operations
- WARN: non-fatal issues (file empty, auth failed)
- ERROR: fatal issues (socket error, file not found)
