#pragma once
#include <atomic>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <unordered_map>

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

  bool registerUser(const std::string &username, const std::string &password);

  bool authenticateUser(const std::string &username, const std::string &password);

  bool userExists(const std::string &username);

  void saveMessageToDB(const std::string &username, const std::string &content);

  void savePrivateMessageToDB(const std::string &sender, const std::string &receiver, const std::string &content);

  void sendHistoryToClient(int client_socket, const std::string &username);

  
  int port_;                        
  int server_fd_;                   
  sockaddr_in server_address_;      
  std::atomic<bool> is_running_{false}; 

  
  std::vector<int> client_sockets_;      
  std::mutex clients_mutex_;             
  std::atomic<int> active_threads_{0};   

  
  std::unordered_map<std::string, int> active_users_; 
};