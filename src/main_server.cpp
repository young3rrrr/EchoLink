#include "network/TCPServer.hpp"

#define PORT 8080

int main() {
  TCPServer server(PORT);

  if (server.start()) {
    server.run();
  }

  return 0;
}