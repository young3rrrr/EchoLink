#pragma once
#include <atomic>
#include <string>
#include <vector>
#include <mutex>

#include <ftxui/component/screen_interactive.hpp>

class TCPClient {
public:
  TCPClient(const std::string &ip, int port);
  ~TCPClient();

  bool connectToServer(); // підключення та запрос імені користувача
  void run();             // Запуск TUI та головного циклу
  void stop();            // Відключення від сервера

private:
  void receiveMessages(); // метод для читання повідомлень у фоні

  std::string ip_;
  int port_;
  int socket_fd_;
  std::string username_;
  std::atomic<bool> is_running_; 

  // --- Нові змінні для інтерактивного TUI ---
  std::vector<std::string> chat_history_; // Тут зберігатимуться всі повідомлення
  std::mutex history_mutex_;              // Захист історії від конфліктів між потоками
  ftxui::ScreenInteractive* screen_;      // Вказівник на екран для його оновлення
};