/**
 * @file main_client.cpp
 * @brief Entry point for the EchoLink TCP chat client application.
 *
 * This file contains the main() function that initializes and runs the
 * EchoLink client. The client connects to a remote TCP server using the
 * default connection parameters and launches the interactive chat interface.
 */

#include "common/Constants.hpp"
#include "network/TCPClient.hpp"

/**
 * @brief Entry point for the EchoLink client application.
 *
 * This function:
 * 1. Creates a TCPClient instance with default server connection parameters
 *    (localhost IP: 127.0.0.1, Port: 8080)
 * 2. Attempts to establish a connection to the server
 * 3. If connection succeeds, starts the interactive chat interface
 *
 * @return 0 on successful execution (normal exit), or if connection fails
 *
 * @note To connect to a remote server, modify the IP address passed to
 *       TCPClient constructor (currently uses DEFAULT_SERVER_IP).
 * @note The client will display appropriate error messages if connection
 *       to the server fails.
 */
int main() {
  // Create a client instance with default server connection parameters
  TCPClient client(echolink::DEFAULT_SERVER_IP, echolink::DEFAULT_PORT);

  // Attempt connection to the server
  if (client.connectToServer()) {
    // Connection successful - start the interactive client
    client.run();
  }

  return 0;
}