/**
 * @file TCPServer.hpp
 * @brief Multi-threaded TCP server implementation for the EchoLink chat application.
 *
 * This class implements a multi-threaded TCP server that:
 * - Accepts incoming client connections on a specified port
 * - Handles user registration and authentication with a PostgreSQL database
 * - Maintains active user sessions and active connections
 * - Routes and broadcasts messages between clients
 * - Manages private message delivery
 * - Persists chat messages and history to a database
 *
 * The server uses POSIX socket API with C++ threads for concurrent client
 * handling and mutex-protected data structures for thread-safe operations.
 */

#pragma once
#include <atomic>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <unordered_map>

/**
 * @class TCPServer
 * @brief A multi-threaded TCP server for EchoLink chat application.
 *
 * This class manages server-side TCP operations including client connections,
 * message routing, user authentication with database persistence, and
 * concurrent message handling through per-client threads.
 */
class TCPServer {
public:
  /**
   * @brief Constructs a TCPServer instance with a specified listening port.
   *
   * Initializes the server with the given port and prepares socket structures
   * for IPv4 (AF_INET) with TCP (SOCK_STREAM) protocol.
   *
   * @param port The port number on which the server will listen for incoming connections.
   */
  explicit TCPServer(int port);

  /**
   * @brief Destructor that ensures proper cleanup of server resources.
   *
   * Calls stop() to close the server socket and terminate all active connections.
   */
  ~TCPServer();

  /**
   * @brief Initializes and starts the TCP server.
   *
   * This method performs the following initialization steps:
   * 1. Creates a TCP socket (AF_INET for IPv4, SOCK_STREAM for TCP)
   * 2. Sets the SO_REUSEADDR socket option to allow port reuse after restart
   * 3. Binds the socket to the specified port address
   * 4. Begins listening for incoming client connections
   * 5. Outputs status messages to the console
   *
   * @return true if the server successfully initialized and started listening,
   *         false if an error occurred at any initialization step.
   *
   * @note On failure, the socket is closed via stop() and an error message
   *       is printed to stderr indicating the nature of the failure.
   */
  bool start();

  /**
   * @brief Runs the server's main event loop for accepting and processing clients.
   *
   * This method:
   * 1. Sets is_running_ to true
   * 2. Spawns a console thread to handle server commands (/stop, exit)
   * 3. Enters an infinite loop that accepts incoming client connections
   * 4. For each accepted client connection:
   *    - Stores the client socket in the client list
   *    - Spawns a new thread to handle that client's communication
   * 5. Waits for all client threads to complete before returning
   *
   * @note This method blocks indefinitely until stop() is called.
   * @note Each client is handled concurrently in its own thread.
   */
  void run();

  /**
   * @brief Gracefully shuts down the server and closes all connections.
   *
   * Sets is_running_ to false, which signals the main loop and receive threads
   * to terminate gracefully. All active client sockets are closed.
   */
  void stop();

private:
  /**
   * @brief Handles communication with a single connected client.
   *
   * This method runs in a separate thread for each client and:
   * 1. Receives and parses authentication commands (/register, /login)
   * 2. Validates credentials against the database
   * 3. Manages the authenticated user session
   * 4. Routes incoming messages to appropriate destinations
   * 5. Persists messages to the database
   * 6. Handles client disconnection and cleanup
   *
   * @param client_socket The file descriptor of the connected client socket.
   *
   * @note This method runs in its own thread and accesses shared data
   *       (client_sockets_, active_users_) through clients_mutex_.
   */
  void handleClient(int client_socket);

  /**
   * @brief Broadcasts a message to all connected clients except the sender.
   *
   * This method sends the specified message to all clients in the client_sockets_
   * list, excluding the client identified by sender_socket. Access to the client
   * list is protected by clients_mutex_ for thread safety.
   *
   * @param message The message text to broadcast.
   * @param sender_socket The socket of the client that sent the message
   *                       (this client will not receive its own message).
   */
  void broadcastMessage(const std::string &message, int sender_socket);

  /**
   * @brief Registers a new user account in the database.
   *
   * @param username The username for the new account.
   * @param password The password for the new account.
   *
   * @return true if registration was successful, false if the username
   *         already exists or a database error occurred.
   */
  bool registerUser(const std::string &username, const std::string &password);

  /**
   * @brief Authenticates a user by verifying credentials against the database.
   *
   * @param username The username to authenticate.
   * @param password The password to verify.
   *
   * @return true if the username exists and the password matches, false otherwise.
   */
  bool authenticateUser(const std::string &username, const std::string &password);

  /**
   * @brief Checks if a user account exists in the database.
   *
   * @param username The username to check for existence.
   *
   * @return true if the user account exists, false otherwise.
   */
  bool userExists(const std::string &username);

  /**
   * @brief Persists a global chat message to the database.
   *
   * @param username The username of the message sender.
   * @param content The message content to store.
   *
   * @note This is used for global chat messages that should be saved for history.
   */
  void saveMessageToDB(const std::string &username, const std::string &content);

  /**
   * @brief Persists a private message to the database.
   *
   * @param sender The username of the message sender.
   * @param receiver The username of the message recipient.
   * @param content The message content to store.
   *
   * @note This is used for private messages that should be saved for history.
   */
  void savePrivateMessageToDB(const std::string &sender, const std::string &receiver, const std::string &content);

  /**
   * @brief Sends chat history to a newly authenticated client.
   *
   * @param client_socket The socket of the client to send history to.
   * @param username The username of the authenticated client (for context).
   *
   * @note This is called after successful authentication to populate client history.
   */
  void sendHistoryToClient(int client_socket, const std::string &username);

  // --- Server Connection Properties ---
  int port_;                        ///< Port number for listening
  int server_fd_;                   ///< Server socket file descriptor
  sockaddr_in server_address_;      ///< Server address structure for socket binding
  std::atomic<bool> is_running_{false}; ///< Flag indicating if server is running

  // --- Client Connection Management ---
  std::vector<int> client_sockets_;      ///< List of connected client socket descriptors
  std::mutex clients_mutex_;             ///< Synchronization mutex for client list access
  std::atomic<int> active_threads_{0};   ///< Counter of active client handling threads

  // --- User Session Management ---
  std::unordered_map<std::string, int> active_users_; ///< Map of username to socket for active sessions
};