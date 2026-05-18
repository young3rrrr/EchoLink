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

  // --- Состояния TUI и Авторизации ---
  std::string username_;
  std::string password_;
  std::atomic<bool> is_authenticated_{false};
  int app_state_ = 0; // 0 = Экран логина, 1 = Экран чата
  std::string auth_status_msg_ = "Welcome! Please log in or register.";

  // --- Данные чатов ---
  std::vector<std::string> chat_tabs_ = {"Global"}; 
  int selected_tab_ = 0;                            
  std::unordered_map<std::string, std::vector<std::string>> chat_histories_;
  std::mutex history_mutex_;              

  // --- Настройки интерфейса ---
  ftxui::ScreenInteractive* screen_;      
  int scroll_offset_ = 0; 

  // --- Модальное окно ---
  bool show_modal_ = false;
  std::string new_chat_name_;
};