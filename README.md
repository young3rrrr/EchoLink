# EchoLink

EchoLink is a high-performance, multi-threaded terminal-based client-server messaging application written in C++. It features real-time communication, a fully interactive Terminal User Interface (TUI), user authentication, private messaging, and a persistent chat history using a PostgreSQL database.

## 🚀 Features

* Interactive TUI: A sleek, responsive terminal interface built with FTXUI, featuring chat tabs, modal dialogs, and message scrolling.
* User Authentication: Secure user registration and login system to protect user sessions.
* Global & Private Messaging: Chat in the global room or send private messages to specific users in dedicated tabs.
* Persistent History: All global and private messages are automatically saved to a PostgreSQL database.
* Comprehensive History Loading: Clients automatically receive their full global and private chat histories upon successful login.
* Multi-Client Support: Handles multiple simultaneous connections using efficient C++ threading.
* Graceful Shutdown: Implements a thread-safe shutdown sequence (/exit or /stop) to ensure no data loss or memory leaks.

## 🧰 Technology Stack

* Language: C++ (C++20)
* Networking: POSIX Sockets (<sys/socket.h>) / TCP/IPv4
* Database: PostgreSQL (via libpqxx C++ client)
* UI Library: FTXUI (Functional Terminal eXtension UI)
* Build System: CMake

## 🏗 Architecture

### Server
* Listens on a specified port and accepts incoming TCP connections.
* Manages a pool of client threads and tracks active user sessions.
* Interfaces with PostgreSQL for user authentication, global message persistence, and private message storage.
* Routes private messages directly to recipients and broadcasts global messages.

### Client
* Connects via IP and Port.
* Uses a dual-threaded approach: one thread for continuous message reception in the background, and one for the FTXUI event loop.
* Features a tab-based interface that seamlessly separates global chat and private conversations.

## ⚙️ Database Setup

To enable user authentication and message persistence, create a PostgreSQL database named echolink_db and execute the following SQL to set up the necessary tables:

```sql
-- Table for user authentication
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password VARCHAR(255) NOT NULL
);

-- Table for global chat history
CREATE TABLE messages (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) NOT NULL,
    content TEXT NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Table for private messaging history
CREATE TABLE private_messages (
    id SERIAL PRIMARY KEY,
    sender_username VARCHAR(50) NOT NULL,
    receiver_username VARCHAR(50) NOT NULL,
    content TEXT NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);