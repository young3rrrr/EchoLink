#include "network/TCPServer.hpp"

/// TCP port number for the server to listen on
#define PORT 8080

int main() {
  TCPServer server(PORT);

  if (server.start()) {
    server.run();
  }

  return 0;
}