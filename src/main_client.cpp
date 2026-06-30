#include "common/Constants.hpp"
#include "network/TCPClient.hpp"

int main() {
  TCPClient client(echolink::DEFAULT_SERVER_IP, echolink::DEFAULT_PORT);

  if (client.connectToServer()) {
    client.run();
  }

  return 0;
}