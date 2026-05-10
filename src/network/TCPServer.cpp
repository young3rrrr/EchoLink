#include "network/TCPServer.hpp"
#include "network/NetworkUtils.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <pqxx/pqxx>
#include <thread>
#include <unistd.h>

const std::string DB_CONN = "dbname=echolink_db user=echolink_user "
                            "password=14341225 hostaddr=127.0.0.1 port=5432";

TCPServer::TCPServer(int port)
    : port_(port), server_fd_(-1),
      is_running_(false) { 
  memset(&server_address_, 0, sizeof(server_address_));
  server_address_.sin_family = AF_INET;
  server_address_.sin_addr.s_addr = INADDR_ANY;
  server_address_.sin_port = htons(port_);
}

TCPServer::~TCPServer() { stop(); }

bool TCPServer::start() {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ == -1) {
    std::cerr << "Error: Failed to create socket.\n";
    return false;
  }

  int opt = 1;
  setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if (bind(server_fd_, (struct sockaddr *)&server_address_,
           sizeof(server_address_)) == -1) {
    std::cerr << "Error: Port " << port_ << " is already in use!\n";
    stop();
    return false;
  }

  if (listen(server_fd_, SOMAXCONN) == -1) {
    std::cerr << "Error: cant start listening\n";
    stop();
    return false;
  }

  std::cout << "=== EchoLink server started ===\n";
  std::cout << "Listening on port " << port_ << "...\n";
  return true;
}

void TCPServer::run() {
  is_running_ = true;

  std::vector<std::thread> client_threads;

  std::thread console_thread([this]() {
    std::string command;
    while (is_running_) {
      std::getline(std::cin, command);

      if (command == "/stop" || command == "exit") {
        std::cout << "[Server] Shutting down...\n";
        stop();
        break;
      } else if (!command.empty()) {
        std::cout << "[Server] Unknown command.\n";
      }
    }
  });

  while (is_running_) {
    sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);

    int client_socket =
        accept(server_fd_, (struct sockaddr *)&client_address, &client_len);

    if (client_socket == -1) {
      if (!is_running_) {
        break;
      }
      std::cerr << "[Error] Failed to accept client.\n";
      continue;
    }

    std::cout << "[Info] New connection! Socket allocated: " << client_socket
              << "\n";

    {
      std::lock_guard<std::mutex> lock(clients_mutex_);
      client_sockets_.push_back(client_socket);
    }

    client_threads.emplace_back(&TCPServer::handleClient, this, client_socket);
  }

  if (console_thread.joinable()) {
    console_thread.join();
  }

  for (auto &th : client_threads) {
    if (th.joinable()) {
      th.join();
    }
  }

  std::cout << "=== Server successfully stopped ===\n";
}

void TCPServer::broadcastMessage(const std::string &message,
                                 int sender_socket) {
  std::lock_guard<std::mutex> lock(clients_mutex_);

  for (int client_fd : client_sockets_) {
    if (client_fd != sender_socket) {
      sendMessage(client_fd, message);
    }
  }
}

void TCPServer::handleClient(int client_socket) {
  bool is_first_message = true;
  bool successfully_joined = false;

  while (true) {
    std::string received_msg;

    if (!receiveMessage(client_socket, received_msg)) {
      if (!is_running_) {
        break;
      }

      std::lock_guard<std::mutex> lock(clients_mutex_);
      client_sockets_.erase(std::remove(client_sockets_.begin(),
                                        client_sockets_.end(), client_socket),
                            client_sockets_.end());
      close(client_socket);

      if (successfully_joined) {
          std::cout << "[Info] Client " << client_socket << " disconnected.\n";
          std::string disconnect_msg = "[Server]: Someone left the chat.";
          for (int fd : client_sockets_) {
            sendMessage(fd, disconnect_msg);
          }
      }
      break;
    }

    successfully_joined = true;

    if (received_msg.find_first_not_of(" \t\n\r\f\v") == std::string::npos) {
      continue;
    }

    std::string username = "System";
    std::string content = received_msg;

    size_t colon_pos = received_msg.find("]: ");
    if (received_msg[0] == '[' && colon_pos != std::string::npos) {
      username = received_msg.substr(1, colon_pos - 1);
      content = received_msg.substr(colon_pos + 3);
    }

    if (username != "Server") {
      saveMessageToDB(username, content);
    }

    broadcastMessage(received_msg, client_socket);

    if (is_first_message) {
      is_first_message = false;
      sendHistoryToClient(client_socket);
    }
  }

  std::cout << "[Debug] Client thread for socket " << client_socket
            << " safely closed.\n";
}

void TCPServer::stop() {
  is_running_ = false;

  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    std::string shutdown_msg = "[Server]: Server is shutting down.";

    for (int fd : client_sockets_) {
      sendMessage(fd, shutdown_msg);
      shutdown(fd, SHUT_RDWR); 
      close(fd);
    }
    client_sockets_.clear();
  }

  if (server_fd_ != -1) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }
}

void TCPServer::saveMessageToDB(const std::string &username,
                                const std::string &content) {
  try {
    pqxx::connection conn(DB_CONN);
    pqxx::work txn(conn);
    txn.exec_params("INSERT INTO messages (username, content) VALUES ($1, $2)",
                    username, content);
    txn.commit();

  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Failed to save message: " << e.what() << "\n";
  }
}

void TCPServer::sendHistoryToClient(int client_socket) {
  try {
    pqxx::connection conn(DB_CONN);
    pqxx::nontransaction txn(conn);

    pqxx::result res = txn.exec(
        "SELECT username, content FROM ("
        "  SELECT id, username, content FROM messages ORDER BY id DESC LIMIT 25"
        ") AS sub ORDER BY id ASC;");

    if (!res.empty()) {
      sendMessage(client_socket, "\n--- Chat History (Last 25 messages) ---");
      for (auto row : res) {
        std::string user = row["username"].c_str();
        std::string text = row["content"].c_str();
        std::string formatted_msg = "[" + user + "]: " + text;
        sendMessage(client_socket, formatted_msg);
      }
      sendMessage(client_socket, "---------------------------------------\n");
    }
  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Failed to fetch history: " << e.what() << "\n";
  }
}