#include "network/TCPServer.hpp"
#include "network/NetworkUtils.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <pqxx/pqxx>
#include <thread>
#include <unistd.h>
#include <sstream>
#include <ranges>


const std::string DB_CONN = "dbname=echolink_db user=echolink_user "
                            "password=14341225 host=echolink_db port=5432";

TCPServer::TCPServer(int port)
    : port_(port), server_fd_(-1), is_running_(false) {
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
    std::cerr << "Error: Failed to start listening.\n";
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

  
  for (int client_fd : active_users_ | std::views::values) {
    if (client_fd != sender_socket) {
      
      sendMessage(client_fd, message);
    }
  }
}

void TCPServer::handleClient(int client_socket) {
  bool is_authenticated = false;
  std::string current_username = "";

  std::cout << "[Info] New connection! Socket: " << client_socket << std::endl;
  sendMessage(client_socket, "[Server]: Welcome! Please authenticate.");

  while (true) {
    std::string received_msg;

    
    if (!receiveMessage(client_socket, received_msg)) {
      if (!is_running_) break;

      

      
      {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        client_sockets_.erase(std::remove(client_sockets_.begin(), client_sockets_.end(), client_socket), client_sockets_.end());
      }
      close(client_socket);

      
      if (is_authenticated) {
        {
          std::lock_guard<std::mutex> lock(clients_mutex_);
          active_users_.erase(current_username);
        }
        std::cout << "[Info] User " << current_username << " disconnected." << std::endl;
        std::string disconnect_msg = "[Server]: " + current_username + " left the chat.";
        broadcastMessage(disconnect_msg, client_socket);
      } else {
        std::cout << "[Info] Unauthenticated client disconnected." << std::endl;
      }
      break;
    }

    
    if (received_msg.find_first_not_of(" \t\n\r\f\v") == std::string::npos) continue;

    
    std::string content = received_msg;
    size_t colon_pos = received_msg.find("]: ");
    if (received_msg[0] == '[' && colon_pos != std::string::npos) {
      content = received_msg.substr(colon_pos + 3);

      
      content = trimBack(content);
    }

    
    if (content.rfind("/register ", 0) == 0) {
      std::istringstream iss(content);
      std::string cmd, user, pass;
      iss >> cmd >> user >> pass;

      if (user.empty() || pass.empty()) {
        sendMessage(client_socket, "[Server]: Error: Invalid registration credentials.");
      } else if (registerUser(user, pass)) {
        sendMessage(client_socket, "[Server]: Registration successful! You can now /login");
      } else {
        sendMessage(client_socket, "[Server]: Error: Username already taken.");
      }
      continue;
    }

    
    if (content.rfind("/login ", 0) == 0) {
      std::istringstream iss(content);
      std::string cmd, user, pass;
      iss >> cmd >> user >> pass;

      if (user.empty() || pass.empty()) {
        sendMessage(client_socket, "[Server]: Error: Invalid login credentials.");
      } else if (authenticateUser(user, pass)) {
        is_authenticated = true;
        current_username = user;
        {
          std::lock_guard<std::mutex> lock(clients_mutex_);
          active_users_[user] = client_socket;
        }
        sendMessage(client_socket, "[Server]: Login successful! Welcome, " + user + "!");
        broadcastMessage("[Server]: " + user + " joined the chat!", client_socket);
        sendHistoryToClient(client_socket, current_username);
      } else {
        sendMessage(client_socket, "[Server]: Error: Invalid username or password.");
      }
      continue;
    }

    
    if (!is_authenticated) {
      sendMessage(client_socket, "[Server]: Access Denied. Please /login first.");
      continue;
    }

    
    if (content.rfind("/check_user ", 0) == 0) {
      std::string target_user = content.substr(12);
      
      target_user.erase(0, target_user.find_first_not_of(" \t"));

      
      target_user = trimBack(target_user);

      if (userExists(target_user)) {
        
        sendMessage(client_socket, "[SYS_TAB_OPEN]" + target_user);
      } else {
        
        sendMessage(client_socket, "[Server]: Error! User '" + target_user + "' does not exist.");
      }
      continue;
    }

    
    if (content.rfind("/msg ", 0) == 0) {
      std::istringstream iss(content);
      std::string cmd, target_user;
      iss >> cmd >> target_user;

      
      std::string private_text;
      std::getline(iss, private_text);

      if (target_user.empty() || private_text.empty() || 
          private_text.find_first_not_of(" \t") == std::string::npos) {
        sendMessage(client_socket, "[Server]: Error: Message cannot be empty.");
        continue;
      }

      
      private_text.erase(0, private_text.find_first_not_of(" \t"));

      
      if (!userExists(target_user)) {
        sendMessage(client_socket, "[Server]: Error! User '" + target_user + "' does not exist.");
        continue;
      }

      
      savePrivateMessageToDB(current_username, target_user, private_text);

      
      std::string formatted_msg = "[PRIVATE_MSG][" + current_username + "][" + current_username + "]: " + private_text;
      std::string formatted_echo = "[PRIVATE_MSG][" + target_user + "][" + current_username + "]: " + private_text;

      
      {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (active_users_.count(target_user)) {
          sendMessage(active_users_[target_user], formatted_msg);
        } else {
          sendMessage(client_socket, "[Server]: User " + target_user + " is offline. Message saved to history.");
        }
      }

      
      sendMessage(client_socket, formatted_echo);
      continue;
    }

    
    saveMessageToDB(current_username, content);
    std::string formatted_msg = "[" + current_username + "]: " + content;
    broadcastMessage(formatted_msg, client_socket);
  }
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

bool TCPServer::userExists(const std::string &username) {
  try {
    pqxx::connection conn(DB_CONN.c_str());
    pqxx::nontransaction txn(conn);
    
    pqxx::result res = txn.exec("SELECT id FROM users WHERE username = " + txn.quote(username));
    return !res.empty();
  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Failed to check user existence: " << e.what() << "\n";
    return false;
  }
}

void TCPServer::saveMessageToDB(const std::string &username,
                                const std::string &content) {
  try {
    pqxx::connection conn(DB_CONN);
    pqxx::work txn(conn);

    
    txn.exec("INSERT INTO messages (username, content) VALUES (" +
             txn.quote(username) + ", " +
             txn.quote(content) + ")");
    txn.commit();

  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Failed to save message: " << e.what() << "\n";
  }
}

void TCPServer::sendHistoryToClient(int client_socket, const std::string &username) {
  try {
    pqxx::connection conn(DB_CONN.c_str());
    pqxx::nontransaction txn(conn);

    
    pqxx::result res = txn.exec("SELECT username, content FROM messages ORDER BY id ASC;");
    for (auto row : res) {
      std::string user = row["username"].c_str();

      
      std::string text = trimBack(row["content"].c_str());

      sendMessage(client_socket, "[" + user + "]: " + text);
    }

    
    pqxx::result priv_res = txn.exec(
      "SELECT sender_username, receiver_username, content FROM private_messages "
      "WHERE sender_username = " + txn.quote(username) +
      " OR receiver_username = " + txn.quote(username) + " ORDER BY id ASC;"
    );

    for (auto row : priv_res) {
      std::string sender = row["sender_username"].c_str();
      std::string receiver = row["receiver_username"].c_str();
      std::string text = trimBack(row["content"].c_str());

      
      std::string target_tab = (sender == username) ? receiver : sender;

      std::string msg = "[PRIVATE_MSG][" + target_tab + "][" + sender + "]: " + text;
      sendMessage(client_socket, msg);
    }
  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Failed to fetch history: " << e.what() << "\n";
  }
}

bool TCPServer::registerUser(const std::string &username, const std::string &password) {
  try {
    pqxx::connection conn(DB_CONN.c_str());
    pqxx::work txn(conn);
    
    txn.exec("INSERT INTO users (username, password) VALUES (" +
             txn.quote(username) + ", " +
             txn.quote(password) + ")");
    txn.commit();
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Registration failed: " << e.what() << "\n";
    return false;
  }
}

bool TCPServer::authenticateUser(const std::string &username, const std::string &password) {
  try {
    pqxx::connection conn(DB_CONN.c_str());
    pqxx::nontransaction txn(conn);
    pqxx::result res = txn.exec("SELECT id FROM users WHERE username = " +
                                txn.quote(username) + " AND password = " +
                                txn.quote(password));
    return !res.empty();
  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Auth failed: " << e.what() << "\n";
    return false;
  }
}

void TCPServer::savePrivateMessageToDB(const std::string &sender, const std::string &receiver, const std::string &content) {
  try {
    pqxx::connection conn(DB_CONN.c_str());
    pqxx::work txn(conn);
    txn.exec("INSERT INTO private_messages (sender_username, receiver_username, content) VALUES (" +
             txn.quote(sender) + ", " +
             txn.quote(receiver) + ", " +
             txn.quote(content) + ")");
    txn.commit();
  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Failed to save private message: " << e.what() << "\n";
  }
}