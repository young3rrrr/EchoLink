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

TCPClient::TCPClient(const std::string &ip, int port)
    : ip_(ip), port_(port), socket_fd_(-1), is_running_(false) {}

TCPClient::~TCPClient() { stop(); }

bool TCPClient::connectToServer() {
  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ == -1) {
    std::cerr << "Failed to create client socket\n";
    return false;
  }

  sockaddr_in server_address;
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;    // IPv4
  server_address.sin_port = htons(port_); 

  if (inet_pton(AF_INET, ip_.c_str(), &server_address.sin_addr) <= 0) {
    std::cerr << "Failed to convert IP address\n";
    stop();
    return false;
  }

  std::cout << "Connecting to EchoLink server (" << ip_ << ":" << port_
            << ")...\n";

  int flags = fcntl(socket_fd_, F_GETFL, 0);
  fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

  int res = connect(socket_fd_, (struct sockaddr *)&server_address,
                    sizeof(server_address));

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
    std::string
        received_msg;

    if (!receiveMessage(socket_fd_, received_msg)) {
      std::cout << "\n[Connection to server lost]\n";
      is_running_ = false;
      break; 
    }

    std::cout << received_msg << "\n";
  }
}

void TCPClient::run() {
  std::thread(&TCPClient::receiveMessages, this).detach();

  std::string message;
  while (is_running_) {
    std::getline(std::cin, message);

    message.erase(0, message.find_first_not_of(
                         " \t\n\r\f\v"));
    message.erase(message.find_last_not_of(" \t\n\r\f\v") +
                  1);

    if (message.empty()) {
      continue;
    }

    if (!is_running_)
      break;

    if (message == "/exit") {
      std::string leave_msg = "[Server]: User " + username_ + " left the chat.";

      sendMessage(socket_fd_, leave_msg);

      stop();
      break;
    }

    std::string formatted_message = "[" + username_ + "]: " + message;

    sendMessage(socket_fd_, formatted_message);
  }
  std::cout << "Client finished working.\n";
}

void TCPClient::stop() {
  if (socket_fd_ != -1) {
    is_running_ = false;
    close(socket_fd_);
    socket_fd_ = -1;
  }
}