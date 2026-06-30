/**
 * @file TCPClient.hpp
 * @brief TCP client implementation for the EchoLink chat application.
 *
 * This class implements a TCP client that:
 * - Connects to a remote TCP server at a specified IP address and port
 * - Handles user authentication (login/registration) with the server
 * - Maintains a connection to receive real-time messages from other users
 * - Provides an interactive terminal user interface (TUI) using FTXUI library
 * - Manages chat tabs (global chat and private message conversations)
 * - Implements thread-safe message history storage
 *
 * The client uses non-blocking socket operations for responsive UI and
 * separates message reception into a dedicated thread to prevent blocking
 * the UI event loop.
 */

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