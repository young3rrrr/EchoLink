#pragma once
#include <atomic>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <vector>

class TCPServer {
public:
  explicit TCPServer(int port);
  ~TCPServer();

  bool start(); 
  void run();  
  void stop();

private:
  void handleClient(int client_socket);
  void broadcastMessage(const std::string &message, int sender_socket);

  int port_;
  int server_fd_;
  sockaddr_in server_address_;

  std::vector<int> client_sockets_;
  std::mutex clients_mutex_;

  std::atomic<bool> is_running_{false};
  void saveMessageToDB(const std::string &username, const std::string &content);
  void sendHistoryToClient(int client_socket);

  std::atomic<int> active_threads_{0};
};