#pragma once
#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

#include <ftxui/component/screen_interactive.hpp>

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
  std::atomic<bool> is_running_;  

  std::string username_;                                          
  std::string password_;                                           
  std::atomic<bool> is_authenticated_{false};                      
  int app_state_ = 0;                                              
  std::string auth_status_msg_ = "Welcome! Please log in or register."; 

  std::vector<std::string> chat_tabs_ = {"Global"};               
  int selected_tab_ = 0;                                          
  std::unordered_map<std::string, std::vector<std::string>> chat_histories_; 
  std::mutex history_mutex_;                                      

  ftxui::ScreenInteractive* screen_;  
  int scroll_offset_ = 0;             

  bool show_modal_ = false;              
  std::string new_chat_name_;            
};