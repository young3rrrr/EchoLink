# EchoLink

EchoLink is a high-performance, multi-threaded terminal-based client-server messaging application written in C++. It features real-time communication, a fully interactive Terminal User Interface (TUI), user authentication, private messaging, and a persistent chat history using a PostgreSQL database.

## 🚀 Features

* **Interactive TUI**: A sleek, responsive terminal interface built with FTXUI, featuring chat tabs, modal dialogs, and message scrolling.
* **User Authentication**: Secure user registration and login system to protect user sessions.
* **Global & Private Messaging**: Chat in the global room or send private messages to specific users in dedicated tabs.
* **Persistent History**: All global and private messages are automatically saved to a PostgreSQL database.
* **Robust Input Handling**: Improved command parsing (e.g., `/exit`, `/stop`) that sanitizes hidden characters and prevents terminal input bugs.
* **Multi-Client Support**: Handles multiple simultaneous connections using efficient C++ threading.
* **Automated Backups**: Daily automated database dumps with a 7-day retention policy using a dedicated cron container.
* **Nginx Reverse Proxy**: Secure traffic routing to the server via a reverse proxy.
* **Dockerized Infrastructure**: Fully containerized deployment for the server, client, database, proxy, and backup services using Docker Compose.

## 🧰 Technology Stack

* **Language**: C++ (C++20)
* **Networking**: POSIX Sockets (`<sys/socket.h>`) / TCP/IPv4
* **Database**: PostgreSQL (via libpqxx C++ client)
* **UI Library**: FTXUI (Functional Terminal eXtension UI)
* **Infrastructure**: Docker, Docker Compose, Nginx, Bash
* **Build System**: CMake

## 🏗 Architecture

### Infrastructure (Docker Compose)
The project is decoupled into isolated microservices for scalability and ease of deployment:
* `echolink_db`: PostgreSQL database for storing users and messages.
* `echolink`: The core C++ server application.
* `reverse_proxy`: Nginx proxy routing incoming connections (Port 80 -> 8080).
* `backup_db`: A standalone container that automatically backs up the database daily.
* `echolink_client`: The C++ client featuring the graphical TUI.

### Server
* Listens on a specified port and accepts incoming TCP connections.
* Manages a pool of client threads and tracks active user sessions.
* Interfaces with PostgreSQL for user authentication, global message persistence, and private message storage.
* Routes private messages directly to recipients and broadcasts global messages.

### Client
* Connects to the server via TCP.
* Uses a dual-threaded approach: one thread for continuous message reception in the background, and one for the FTXUI event loop.
* Features a tab-based interface that seamlessly separates global chat and private conversations.

## ⚙️ Deployment & Getting Started

The project is fully containerized, removing the need for manual database or dependency setup. You can deploy it in two configurations using Docker Compose:

### Option 1: Full Stack (Server + Client)
If you want to run the entire infrastructure including the local TUI client (great for testing):
```bash
docker compose up -d --build
```
### Option 2: Server-Only (Production Mode)
```bash
docker compose up -d --build echolink_db echolink reverse_proxy backup_db
```

## 🗄️ Automated Backups

EchoLink features an autonomous backup system. A dedicated container automatically creates compressed PostgreSQL dumps (.dump format) every day at 02:00 AM.

* **Location**: Backup archives are saved on the host machine in the deploy/postgres/backup/ directory (which is safely ignored by Git).
* **Retention**: A built-in cleanup script automatically deletes archives older than 7 days to prevent disk space exhaustion.