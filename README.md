# LocalBin

### A Distributed File Sharing Cloud Server with Hybrid C/Python Architecture

---

## Overview

**LocalBin** is a **socket based distributed file sharing system** designed to simulate a small scale cloud storage environment.
It allows multiple clients to securely **upload**, **download**, **delete**, and **manage directories** on a remote server.

The project combines **high-performance C networking** with **Python-based analytics and user interface**, providing both low level efficiency and high level flexibility.

---

## System Architecture

LocalBin uses a **Client–Server model** with three main layers:

```
[ Python CLI (User Interface) ]
        │
        ▼
[ C Client Library (libclient.so) ]
        │
        ▼
[ C Multithreaded Server ]
        │
        ▼
[ File Storage + Logging ]
        │
        ▼
[ Python Data Analysis (Metrics, Graphs, Reports) ]
```

* **Server (C)** – Handles socket connections, authentication, and file operations.
* **Client Library (C)** – Exposes low level communication routines via a shared object (`libclient.so`).
* **Interface (Python)** – Provides a CLI and connects to the C client via `ctypes`.
* **Analytics (Python)** – Parses logs, measures performance, and visualizes results.

--

## License

MIT License © 2025 – Our Dev Team
This project was developed for educational use as part of CNT3004 – Socket-Based Networked File Sharing Cloud Server.
