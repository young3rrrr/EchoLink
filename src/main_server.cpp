/**
 * @file main_server.cpp
 * @brief Entry point for the EchoLink TCP chat server application.
 *
 * This file contains the main() function that initializes and runs the
 * EchoLink server. The server listens for incoming client connections on
 * a specified port and handles concurrent multi-user chat communication.
 */

#include "network/TCPServer.hpp"

/// TCP port number for the server to listen on
#define PORT 8080

/**
 * @brief Entry point for the EchoLink server application.
 *
 * This function:
 * 1. Creates a TCPServer instance listening on the specified port (8080)
 * 2. Initializes the server socket and binds to the port
 * 3. If initialization succeeds, enters the main server event loop
 *
 * @return 0 on successful execution (normal shutdown), or if server
 *         initialization fails
 *
 * @note The server will continue running and accepting connections until
 *       the user enters the "/stop" or "exit" command in the console.
 * @note Database connectivity is required for user authentication and
 *       message persistence (PostgreSQL on localhost).
 */
int main() {
  // Create and start the TCP server on the specified port
  TCPServer server(PORT);

  // Initialize server socket, bind to port, and start listening
  if (server.start()) {
    // Server started successfully - enter main event loop
    server.run();
  }

  return 0;
}