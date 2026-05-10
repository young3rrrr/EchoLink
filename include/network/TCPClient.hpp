#pragma once
#include <atomic>
#include <string>

class TCPClient {
public:
  TCPClient(const std::string &ip, int port);
  ~TCPClient();

  bool connectToServer();
  void run();
  void stop();

private:
  void receiveMessages();

  std::string ip_;
  int port_;
  int socket_fd_;
  std::string username_;
  std::atomic<bool> is_running_;
};
