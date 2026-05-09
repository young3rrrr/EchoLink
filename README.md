# EchoLink

EchoLink is a high-performance, multi-threaded terminal-based client-server messaging application written in C++. It features real-time communication between multiple users over a TCP/IP network and maintains a persistent chat history using a PostgreSQL database.

## 🚀 Features

*   **Real-Time Messaging:** Instant message broadcasting across all connected clients.
*   **Persistent History:** All messages are automatically saved to a PostgreSQL database.
*   **Smart History Loading:** New clients automatically receive the last 25 messages upon joining to provide context.
*   **Multi-Client Support:** Handles multiple simultaneous connections using efficient C++ threading.
*   **Graceful Shutdown:** Implements a thread-safe shutdown sequence (`exit` or `/stop`) to ensure no data loss or memory leaks.
*   **Clean Codebase:** Pre-configured with `clang-format` for consistent styling and a robust `.gitignore`.

## 🧰 Technology Stack

*   **Language:** C++ (C++11 or higher)
*   **Networking:** 
    *   POSIX Sockets (`<sys/socket.h>`) for Linux/Unix
    *   Protocol: TCP/IPv4
*   **Database:**
    *   PostgreSQL
    *   Library: `libpqxx` (C++ client for PostgreSQL)
*   **Build System:** CMake

## 🏗 Architecture

### Server
*   Listens on a specified port and accepts incoming TCP connections.
*   Manages a pool of client threads.
*   Interfaces with PostgreSQL to store and retrieve messages.
*   Broadcasts messages to all active users while excluding the sender to prevent echoes.

### Client
*   Connects via IP and Port.
*   Uses a dual-threaded approach: one thread for continuous message reception and one for user input.
*   Supports custom nicknames and system commands.

## ⚙️ Database Setup

To enable message persistence, create a PostgreSQL database named `echolink_db` and execute the following SQL to set up the table:

```sql
CREATE TABLE messages (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) NOT NULL,
    content TEXT NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);