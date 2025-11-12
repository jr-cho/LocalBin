# LocalBin – Socket-Based Networked File Sharing Cloud Server

LocalBin is a high-performance distributed file sharing system designed for cloud environments with a robust server-client architecture. It employs a hybrid architecture that separates performance-critical operations (networking, file I/O, and threading) into a C backend, while exposing a user-friendly Python frontend for CLI interaction via a Foreign Function Interface (FFI).

## Project Scope

LocalBin allows multiple clients (running on separate computers) to connect to a central server to:
- Securely authenticate users with encrypted credentials (no clear-text password transmission)
- Upload, download, delete, and manage files within the server's storage system
- Navigate and manage subdirectories on the server
- View directory listings of available files and folders
- Track performance metrics including upload/download rates and transfer times

### Key Objectives

1. Implement a multithreaded TCP server in C that handles concurrent client connections efficiently
2. Support various file types with minimum sizes:
   - Text files: 25 MB
   - Audio files: 1 GB
   - Video files: 2 GB
3. Create a dedicated C client module with socket-based operations
4. Provide a Python frontend CLI that interacts with the C backend via FFI
5. Implement comprehensive error handling and informative error messages
6. Collect and analyze performance metrics (upload/download data rates, transfer times, system response times)
7. Use structured, protocol-driven communication for reliable client-server interaction

## System Architecture

| Layer | Language | Description |
| :--- | :--- | :--- |
| **C Backend (Core)** | C | Handles networking, multithreading, file I/O, authentication, and logging |
| **Python Frontend (Interface)** | Python | Provides CLI interface, interacting with backend via FFI |
| **Analysis Module** | Python | Collects and stores performance statistics for offline analysis |
| **Network Layer** | TCP/IP | Client-server communication using TCP protocol (UDP optional) |

**Note:** FTP is not permitted. Server and client processes must run on separate computers.

## Client Operations

The system implements the following client commands:

- **Connect [server IP] [Port]** - Initiates secure connection with authentication
- **Upload [filename]** - Upload files to server (prompts if file exists)
- **Download [filename]** - Download files from server (error if not found)
- **Delete [filename]** - Delete files from server (error if file is in use or doesn't exist)
- **Dir** - List files and subdirectories in server storage
- **Subfolder [create|delete] [path/directory]** - Create or delete subdirectories

## Project Structure

```bash
localbin/
│
├── core/
│   ├── common/
│   │   ├── common.h             # Shared constants, macros, logging, recv/send helpers
│   │   ├── common.c             # Implementation of common functions
│   │   ├── protocol.h           # Message types, CommandType, MessageHeader
│   │   └── protocol.c           # Protocol serialization, deserialization, send/recv
│   │
│   ├── server/
│   │   ├── server.h             # Server struct, lifecycle, client handler
│   │   ├── server.c             # Multithreaded server implementation
│   │   ├── auth.h               # Authentication with encryption
│   │   ├── auth.c
│   │   ├── file_ops.h           # File operations: upload, download, delete, list
│   │   └── file_ops.c
│   │
│   └── client/
│       ├── client.h             # Client socket interface
│       └── client.c             # Client connection and command logic
│
├── python/
│   ├── main.py                  # Frontend CLI entry point
│   ├── analysis.py              # Performance metrics collection module
│   └── ffi/
│       ├── __init__.py
│       └── bridge.py            # ctypes/cffi bindings to C backend
│
└── storage/                     # Server file storage directory
    └── users/                   # User-specific directories
```

## Performance Metrics

The analysis module tracks:
- Upload and download data rates (MB/s)
- File transfer times
- System response times
- Statistics stored in dictionary/dataframe format for offline analysis

## File Naming Convention

Files are organized using logical naming conventions that identify file type:
- Text files: TS001, TS002, etc. (Text-Server)
- Audio files: AS001, AS002, etc. (Audio-Server)
- Video files: VS001, VS002, etc. (Video-Server)

## Security Features

- Secure authentication mechanism (username/password)
- Password encryption (no clear-text transmission)
- Authorization checks for file operations
- Error handling for unauthorized access attempts

## Development Requirements

- **Language:** Python (frontend/analysis) and C (backend/core)
- **Protocol:** TCP/IP (primary), UDP (optional)
- **Minimum Modules:** 3 Python modules (client, server wrapper, analysis)
- **Deployment:** Server and clients must run on separate computers

## Testing

All testing should be performed from the client-side application issuing specific commands:
- Connect and authenticate
- Upload files (various types and sizes)
- Download files
- Directory listing
- Subfolder creation/deletion
- Delete operations
- Performance measurement under various load conditions

## License

MIT License © 2025 – Our Dev Team  
This project was developed for educational use as part of CNT3004 – Socket-Based Networked File Sharing Cloud Server.
