#pragma once
#include <atomic>
#include <string>

class TCPClient {
public:
  TCPClient(const std::string &ip, int port);
  ~TCPClient();

  bool connectToServer(); // підключення та запрос імені користувача
  void run();             // Запуск цикла відправки та потока читання
  void stop();            // Відключення від сервера та зупинка потока

private:
  void receiveMessages(); // метод для читання повідомлень від сервера в
                          // окремому потоці

  std::string ip_;
  int port_;
  int socket_fd_;
  std::string username_;
  std::atomic<bool> is_running_; // флаг для контролю роботи потока читання
};
