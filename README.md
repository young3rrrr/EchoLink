# EchoLink

EchoLink is a terminal-based client-server messaging application written in C++. It demonstrates real-time communication between multiple users over a TCP/IP network using a centralized server.

## 🚀 Features

* Real-time text messaging
* Multi-client support
* Centralized message broadcasting
* Terminal-based interface (CLI)
* Thread-safe server handling multiple clients simultaneously

## 🧰 Technology Stack

* Language: C++ (C++11 or higher)

* Networking:

  * POSIX sockets (<sys/socket.h>) for Linux/Unix
  * Winsock2 for Windows
  * Protocol: TCP/IPv4

## 🏗 Architecture

### Server

* Binds to a specified port and listens for incoming connections
* Accepts multiple clients
* Handles each client in a separate thread
* Broadcasts incoming messages to all connected users

### Client

* Connects to the server via IP and port
* Allows the user to set a nickname
* Uses separate threads for:

  * Sending messages
  * Receiving messages
* Displays incoming messages in real time

## 📦 How It Works

1. Start the server
2. Run one or more client instances
3. Each client connects to the server and enters a nickname
4. Messages sent by any client are broadcast to all connected users

## 📚 Learning Goals

This project demonstrates:

* Low-level network programming
* Multithreading in C++
* Client-server architecture
* Synchronization and thread safety
