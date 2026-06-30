/**
 * @file TCPServer.cpp
 * @brief Implementation of a multi-threaded TCP server for the EchoLink chat application.
 *
 * This file contains the implementation of a multi-threaded TCP server that:
 * - Accepts incoming client connections on a specified port
 * - Handles user registration and authentication with a PostgreSQL database
 * - Manages active user sessions and broadcasts messages between clients
 * - Handles private messaging between authenticated users
 * - Persists all chat messages and conversation history to a database
 *
 * The server uses POSIX socket API for network communication and C++ standard
 * library threads for concurrent per-client message handling. All shared data
 * structures are protected by mutexes to ensure thread-safe operations.
 */

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

/// Database connection string for PostgreSQL
const std::string DB_CONN = "dbname=echolink_db user=echolink_user "
                            "password=14341225 host=echolink_db port=5432";

/**
 * @brief Constructs a TCPServer instance with the specified listening port.
 *
 * Initializes all server member variables and prepares the server address
 * structure for binding to the specified port.
 *
 * @param port The TCP port number on which the server will listen for client connections.
 *
 * Implementation Details:
 * - Sets port_ to the specified port number
 * - Initializes server_fd_ to -1 (indicates no active socket)
 * - Clears the server_address_ structure
 * - Configures the address structure for IPv4 (AF_INET)
 * - Sets INADDR_ANY to accept connections on all available network interfaces
 * - Converts the port to network-byte-order using htons()
 */
TCPServer::TCPServer(int port)
    : port_(port), server_fd_(-1), is_running_(false) {
  memset(&server_address_, 0, sizeof(server_address_));
  server_address_.sin_family = AF_INET;
  server_address_.sin_addr.s_addr = INADDR_ANY;
  server_address_.sin_port = htons(port_);
}

/**
 * @brief Destructor that ensures proper cleanup of server resources.
 *
 * Calls stop() to close the server socket and all client connections
 * when the TCPServer object is destroyed.
 */
TCPServer::~TCPServer() { stop(); }

/**
 * @brief Initializes and starts the TCP server for accepting client connections.
 *
 * This method performs the following initialization steps:
 * 1. Creates a TCP socket (AF_INET for IPv4, SOCK_STREAM for TCP)
 * 2. Sets the SO_REUSEADDR socket option for port reuse on restart
 * 3. Binds the socket to the server address and port
 * 4. Begins listening for incoming client connections
 * 5. Outputs startup status messages to the console
 *
 * @return true if server successfully initialized and is listening,
 *         false if an error occurred at any initialization step.
 *
 * @note On failure, stop() is called to clean up resources and an error
 *       message is printed to stderr describing the failure cause.
 */
bool TCPServer::start() {
  // Create a TCP socket for IPv4
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ == -1) {
    std::cerr << "Error: Failed to create socket.\n";
    return false;
  }

  // Set SO_REUSEADDR socket option to allow port reuse after restart
  // This prevents "Address already in use" errors after server shutdown
  int opt = 1;
  setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // Bind the socket to the server address and port
  if (bind(server_fd_, (struct sockaddr *)&server_address_,
           sizeof(server_address_)) == -1) {
    std::cerr << "Error: Port " << port_ << " is already in use!\n";
    stop();
    return false;
  }

  // Begin listening for incoming client connections
  // SOMAXCONN is the maximum number of pending connections in the queue
  if (listen(server_fd_, SOMAXCONN) == -1) {
    std::cerr << "Error: Failed to start listening.\n";
    stop();
    return false;
  }

  // Output startup success message to console
  std::cout << "=== EchoLink server started ===\n";
  std::cout << "Listening on port " << port_ << "...\n";
  return true;
}

/**
 * @brief Runs the server's main event loop for accepting and processing client connections.
 *
 * This method:
 * 1. Sets is_running_ to true to signal the server is active
 * 2. Spawns a console thread to handle server commands (/stop, exit)
 * 3. Enters an infinite loop that accepts incoming client connections
 * 4. For each accepted connection:
 *    - Adds the client socket to the client list (thread-safe)
 *    - Spawns a new thread to handle that client's communication
 * 5. Waits for all client threads to complete before returning
 *
 * @note This method blocks indefinitely until stop() is called.
 * @note Each client is handled concurrently in its own dedicated thread.
 */
void TCPServer::run() {
  is_running_ = true;

  std::vector<std::thread> client_threads;

  // Spawn console thread for handling server control commands
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

  // Main event loop for accepting incoming client connections
  while (is_running_) {
    sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);

    // Accept an incoming client connection
    int client_socket =
        accept(server_fd_, (struct sockaddr *)&client_address, &client_len);

    if (client_socket == -1) {
      if (!is_running_) {
        break;
      }
      std::cerr << "[Error] Failed to accept client.\n";
      continue;
    }

    // Add client socket to the thread-safe client list
    {
      std::lock_guard<std::mutex> lock(clients_mutex_);
      client_sockets_.push_back(client_socket);
    }

    // Spawn a new thread to handle this client
    client_threads.emplace_back(&TCPServer::handleClient, this, client_socket);
  }

  // Wait for console thread to terminate
  if (console_thread.joinable()) {
    console_thread.join();
  }

  // Wait for all client handler threads to complete
  for (auto &th : client_threads) {
    if (th.joinable()) {
      th.join();
    }
  }

  std::cout << "=== Server successfully stopped ===\n";
}

/**
 * @brief Broadcasts a message to all connected clients except the sender.
 *
 * This method sends the provided message to all clients in the client_sockets_
 * list, excluding the client identified by sender_socket. Access to the client
 * list is protected by clients_mutex_ for thread-safe concurrent access.
 *
 * @param message The message text to broadcast to all connected clients.
 * @param sender_socket The socket descriptor of the client that sent the message.
 *                       This client will not receive its own message.
 *
 * @note Thread-safe: acquires clients_mutex_ during execution.
 */
void TCPServer::broadcastMessage(const std::string &message,
                                 int sender_socket) {
  // Protect client list access with mutex for thread safety
  std::lock_guard<std::mutex> lock(clients_mutex_);

  // Send message to all connected clients except the sender
  for (int client_fd : active_users_ | std::views::values) {
    if (client_fd != sender_socket) {
      // Use the network utility function for reliable message transmission
      sendMessage(client_fd, message);
    }
  }
}

/**
 * @brief Handles communication with a single connected client.
 *
 * This method runs in a separate thread for each client and:
 * 1. Receives and parses authentication commands (/register, /login)
 * 2. Validates credentials against the PostgreSQL database
 * 3. Manages authenticated user session and active user tracking
 * 4. Handles message routing (global and private messages)
 * 5. Persists all messages to the database
 * 6. Handles client disconnection and cleanup of resources
 *
 * @param client_socket The socket file descriptor for this client connection.
 *
 * @note This method runs in its own thread and accesses shared data structures
 *       (client_sockets_, active_users_) through clients_mutex_.
 * @note The method handles all protocol parsing including special system messages
 *       for tab creation and private message routing.
 */
void TCPServer::handleClient(int client_socket) {
  bool is_authenticated = false;
  std::string current_username = "";

  std::cout << "[Info] New connection! Socket: " << client_socket << std::endl;
  sendMessage(client_socket, "[Server]: Welcome! Please authenticate.");

  while (true) {
    std::string received_msg;

    // Attempt to receive a message from the client
    if (!receiveMessage(client_socket, received_msg)) {
      if (!is_running_) break;

      // Client disconnected - cleanup: remove socket and user from active lists

      // 1. Remove socket from client list with mutex protection
      {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        client_sockets_.erase(std::remove(client_sockets_.begin(), client_sockets_.end(), client_socket), client_sockets_.end());
      }
      close(client_socket);

      // 2. Remove authenticated user from active users with mutex protection
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

    // Skip empty or whitespace-only messages
    if (received_msg.find_first_not_of(" \t\n\r\f\v") == std::string::npos) continue;

    // Parse message content, stripping sender prefix if present
    std::string content = received_msg;
    size_t colon_pos = received_msg.find("]: ");
    if (received_msg[0] == '[' && colon_pos != std::string::npos) {
      content = received_msg.substr(colon_pos + 3);

      // Remove trailing whitespace and newline characters
      content = trimBack(content);
    }

    // ===== AUTHENTICATION HANDLER: REGISTRATION =====
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

    // ===== AUTHENTICATION HANDLER: LOGIN =====
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

    // ===== ACCESS CONTROL: REQUIRE AUTHENTICATION =====
    if (!is_authenticated) {
      sendMessage(client_socket, "[Server]: Access Denied. Please /login first.");
      continue;
    }

    // ===== HANDLER: USER EXISTENCE CHECK (FOR PRIVATE CHAT INITIATION) =====
    if (content.rfind("/check_user ", 0) == 0) {
      std::string target_user = content.substr(12);
      // Remove leading whitespace
      target_user.erase(0, target_user.find_first_not_of(" \t"));

      // Remove trailing whitespace and control characters
      target_user = trimBack(target_user);

      if (userExists(target_user)) {
        // User exists - send signal to create private chat tab
        sendMessage(client_socket, "[SYS_TAB_OPEN]" + target_user);
      } else {
        // User does not exist - send error to current global chat
        sendMessage(client_socket, "[Server]: Error! User '" + target_user + "' does not exist.");
      }
      continue;
    }

    // ===== HANDLER: PRIVATE MESSAGES =====
    if (content.rfind("/msg ", 0) == 0) {
      std::istringstream iss(content);
      std::string cmd, target_user;
      iss >> cmd >> target_user;

      // Extract message text (rest of the line after username)
      std::string private_text;
      std::getline(iss, private_text);

      if (target_user.empty() || private_text.empty() || 
          private_text.find_first_not_of(" \t") == std::string::npos) {
        sendMessage(client_socket, "[Server]: Error: Message cannot be empty.");
        continue;
      }

      // Remove leading whitespace from message
      private_text.erase(0, private_text.find_first_not_of(" \t"));

      // Verify target user exists
      if (!userExists(target_user)) {
        sendMessage(client_socket, "[Server]: Error! User '" + target_user + "' does not exist.");
        continue;
      }

      // Save message to database for persistence
      savePrivateMessageToDB(current_username, target_user, private_text);

      // Format private message for both sender and receiver
      std::string formatted_msg = "[PRIVATE_MSG][" + current_username + "][" + current_username + "]: " + private_text;
      std::string formatted_echo = "[PRIVATE_MSG][" + target_user + "][" + current_username + "]: " + private_text;

      // Send to recipient if online, otherwise notify sender
      {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (active_users_.count(target_user)) {
          sendMessage(active_users_[target_user], formatted_msg);
        } else {
          sendMessage(client_socket, "[Server]: User " + target_user + " is offline. Message saved to history.");
        }
      }

      // Send echo back to sender
      sendMessage(client_socket, formatted_echo);
      continue;
    }

    // ===== DEFAULT HANDLER: GLOBAL CHAT MESSAGE =====
    saveMessageToDB(current_username, content);
    std::string formatted_msg = "[" + current_username + "]: " + content;
    broadcastMessage(formatted_msg, client_socket);
  }
}

/**
 * @brief Gracefully shuts down the server and closes all active connections.
 *
 * This method:
 * 1. Sets is_running_ to false to signal all loops to terminate
 * 2. Notifies all connected clients of the server shutdown
 * 3. Closes all client connections gracefully
 * 4. Closes the server socket
 *
 * This method is safe to call multiple times as it checks socket validity.
 */
void TCPServer::stop() {
  is_running_ = false;

  // Notify all connected clients and close their connections
  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    std::string shutdown_msg = "[Server]: Server is shutting down.";

    for (int fd : client_sockets_) {
      sendMessage(fd, shutdown_msg);
      // Force close read and write operations
      shutdown(fd, SHUT_RDWR);
      // Close the client socket
      close(fd);
    }
    client_sockets_.clear();
  }

  // Close the server socket if it's still open
  if (server_fd_ != -1) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }
}

/**
 * @brief Checks if a user account exists in the database.
 *
 * @param username The username to check for existence.
 *
 * @return true if the user account exists in the database, false otherwise
 *         or if a database error occurs.
 */
bool TCPServer::userExists(const std::string &username) {
  try {
    pqxx::connection conn(DB_CONN.c_str());
    pqxx::nontransaction txn(conn);
    // Query for the user ID
    pqxx::result res = txn.exec("SELECT id FROM users WHERE username = " + txn.quote(username));
    return !res.empty();
  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Failed to check user existence: " << e.what() << "\n";
    return false;
  }
}

/**
 * @brief Persists a global chat message to the PostgreSQL database.
 *
 * This function inserts a chat message into the messages table with the sender's
 * username and message content. The message is automatically timestamped by the
 * database.
 *
 * @param username The username of the message sender.
 * @param content The message content to store.
 *
 * @note Uses parameterized queries to prevent SQL injection attacks.
 * @note Silently logs database errors to stderr but does not throw exceptions.
 */
void TCPServer::saveMessageToDB(const std::string &username,
                                const std::string &content) {
  try {
    pqxx::connection conn(DB_CONN);
    pqxx::work txn(conn);

    // Insert message using parameterized query for security
    txn.exec("INSERT INTO messages (username, content) VALUES (" +
             txn.quote(username) + ", " +
             txn.quote(content) + ")");
    txn.commit();

  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Failed to save message: " << e.what() << "\n";
  }
}

/**
 * @brief Sends chat history to a newly authenticated client.
 *
 * This method retrieves and sends:
 * 1. Complete global chat message history (all messages in order)
 * 2. Private message history where the client is sender or recipient
 *
 * @param client_socket The socket of the authenticated client.
 * @param username The username of the authenticated client.
 *
 * @note Messages are sent in chronological order based on database id.
 */
void TCPServer::sendHistoryToClient(int client_socket, const std::string &username) {
  try {
    pqxx::connection conn(DB_CONN.c_str());
    pqxx::nontransaction txn(conn);

    // 1. Send complete global chat history (all messages)
    pqxx::result res = txn.exec("SELECT username, content FROM messages ORDER BY id ASC;");
    for (auto row : res) {
      std::string user = row["username"].c_str();

      // Remove trailing whitespace and control characters
      std::string text = trimBack(row["content"].c_str());

      sendMessage(client_socket, "[" + user + "]: " + text);
    }

    // 2. Send private message history (messages where user is sender or recipient)
    pqxx::result priv_res = txn.exec(
      "SELECT sender_username, receiver_username, content FROM private_messages "
      "WHERE sender_username = " + txn.quote(username) +
      " OR receiver_username = " + txn.quote(username) + " ORDER BY id ASC;"
    );

    for (auto row : priv_res) {
      std::string sender = row["sender_username"].c_str();
      std::string receiver = row["receiver_username"].c_str();
      std::string text = trimBack(row["content"].c_str());

      // Determine target tab based on message direction
      std::string target_tab = (sender == username) ? receiver : sender;

      std::string msg = "[PRIVATE_MSG][" + target_tab + "][" + sender + "]: " + text;
      sendMessage(client_socket, msg);
    }
  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Failed to fetch history: " << e.what() << "\n";
  }
}

/**
 * @brief Registers a new user account in the database.
 *
 * This function inserts a new user into the users table with the provided
 * username and password.
 *
 * @param username The username for the new account.
 * @param password The password for the new account.
 *
 * @return true if registration succeeded, false if username already exists
 *         or a database error occurred.
 *
 * @note Uses parameterized queries to prevent SQL injection.
 * @warning For production, passwords should be hashed with bcrypt or similar.
 */
bool TCPServer::registerUser(const std::string &username, const std::string &password) {
  try {
    pqxx::connection conn(DB_CONN.c_str());
    pqxx::work txn(conn);
    // Insert new user record
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

/**
 * @brief Authenticates a user by verifying credentials against the database.
 *
 * This function queries the database for a user with matching username and
 * password.
 *
 * @param username The username to authenticate.
 * @param password The password to verify.
 *
 * @return true if the username exists and the password matches, false otherwise
 *         or if a database error occurred.
 *
 * @note Uses parameterized queries to prevent SQL injection.
 */
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

/**
 * @brief Persists a private message to the PostgreSQL database.
 *
 * This function inserts a private message into the private_messages table
 * with sender, receiver, and message content. The message is timestamped
 * automatically by the database.
 *
 * @param sender The username of the message sender.
 * @param receiver The username of the message recipient.
 * @param content The message content to store.
 *
 * @note Uses parameterized queries to prevent SQL injection.
 * @note Silently logs database errors to stderr.
 */
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