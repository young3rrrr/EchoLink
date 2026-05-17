#include "network/TCPClient.hpp"
#include "network/NetworkUtils.h"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/select.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

// --- Підключаємо модулі FTXUI ---
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

using namespace ftxui;

TCPClient::TCPClient(const std::string &ip, int port)
    : ip_(ip), port_(port), socket_fd_(-1), is_running_(false), screen_(nullptr) {}

TCPClient::~TCPClient() { stop(); }

bool TCPClient::connectToServer() {
  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ == -1) {
    std::cerr << "Failed to create client socket\n";
    return false;
  }

  sockaddr_in server_address;
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port_);

  if (inet_pton(AF_INET, ip_.c_str(), &server_address.sin_addr) <= 0) {
    std::cerr << "Failed to convert IP address\n";
    stop();
    return false;
  }

  std::cout << "Connecting to EchoLink server (" << ip_ << ":" << port_ << ")...\n";

  int flags = fcntl(socket_fd_, F_GETFL, 0);
  fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

  int res = connect(socket_fd_, (struct sockaddr *)&server_address, sizeof(server_address));

  if (res < 0) {
    if (errno == EINPROGRESS) {
      fd_set write_set;
      FD_ZERO(&write_set);
      FD_SET(socket_fd_, &write_set);

      struct timeval timeout;
      timeout.tv_sec = 5;
      timeout.tv_usec = 0;

      res = select(socket_fd_ + 1, NULL, &write_set, NULL, &timeout);

      if (res == 0) {
        std::cerr << "Connection timed out! The server is unreachable.\n";
        stop();
        return false;
      } else if (res > 0) {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error != 0) {
          std::cerr << "Connection failed.\n";
          stop();
          return false;
        }
      } else {
        std::cerr << "Select error during connection.\n";
        stop();
        return false;
      }
    } else {
      std::cerr << "Failed to connect to server immediately!\n";
      stop();
      return false;
    }
  }

  fcntl(socket_fd_, F_SETFL, flags);
  std::cout << "Success! Connection with server established.\n";

  std::cout << "Enter your username: ";
  std::getline(std::cin, username_);

  std::string join_msg = "[Server]: User " + username_ + " joined the chat!";
  sendMessage(socket_fd_, join_msg);

  is_running_ = true;
  return true;
}

void TCPClient::receiveMessages() {
  while (is_running_) {
    std::string received_msg;
    
    if (!receiveMessage(socket_fd_, received_msg)) {
      {
        std::lock_guard<std::mutex> lock(history_mutex_);
        chat_history_.push_back("[Connection to server lost]");
      }
      is_running_ = false;
      // Кажемо екрану перемалюватися
      if (screen_) screen_->PostEvent(Event::Custom); 
      break;
    }

    {
      std::lock_guard<std::mutex> lock(history_mutex_);
      chat_history_.push_back(received_msg);
    }
    // Кажемо екрану перемалюватися після отримання нового повідомлення
    if (screen_) screen_->PostEvent(Event::Custom);
  }
}

void TCPClient::run() {
  // Запускаємо потік отримання повідомлень
  std::thread(&TCPClient::receiveMessages, this).detach();

  // Ініціалізуємо екран
  auto screen = ScreenInteractive::Fullscreen();
  screen_ = &screen; // Зберігаємо вказівник для фонового потоку

  std::string input_text;

  // Налаштування поля вводу
  InputOption option;
  option.on_enter = [&] {
    if (input_text.empty()) return;

    if (input_text == "/exit" || input_text == "/stop") {
      std::string leave_msg = "[Server]: User " + username_ + " left the chat.";
      sendMessage(socket_fd_, leave_msg);
      stop();
      screen.Exit(); // Коректно закриваємо графічний інтерфейс
      return;
    }

    std::string formatted_message = "[" + username_ + "]: " + input_text;
    sendMessage(socket_fd_, formatted_message);

    // Додаємо власне повідомлення в локальну історію
    {
      std::lock_guard<std::mutex> lock(history_mutex_);
      chat_history_.push_back(formatted_message);
    }

    input_text.clear(); // Очищаємо поле вводу
  };

  Component input_field = Input(&input_text, "Type message...", option);

  // Створюємо верстку (Renderer)
  auto renderer = Renderer(input_field, [&] {
    Elements history_elements;
    {
      std::lock_guard<std::mutex> lock(history_mutex_);
      // Перетворюємо рядки історії на текстові елементи FTXUI
      for (const auto& msg : chat_history_) {
        history_elements.push_back(text(msg));
      }
    }

    // Вікно чату: вертикальний список з прокруткою (yframe)
    auto history_box = vbox(std::move(history_elements)) | yframe | yflex;

    // Головна структура екрану
    return vbox({
      text(" EchoLink TUI Chat ") | bold | center, // Заголовок
      separator(),                                 // Лінія
      history_box,                                 // Історія повідомлень (займає весь вільний простір)
      separator(),                                 // Лінія
      hbox({text(" " + username_ + " > ") | color(Color::Green), input_field->Render()}) // Рядок вводу
    }) | border; // Огортаємо все в рамку
  });

  // ЗАПУСК ГРАФІКИ (Блокує потік, поки користувач не натисне /exit або Ctrl+C)
  screen.Loop(renderer);

  screen_ = nullptr;
  std::cout << "\nClient closed.\n";
}

void TCPClient::stop() {
  if (socket_fd_ != -1) {
    is_running_ = false;
    close(socket_fd_);
    socket_fd_ = -1;
  }
}