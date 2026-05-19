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

/**
 * @class TCPClient
 * @brief A TCP client for real-time chat communication with user authentication.
 *
 * This class manages the client-side TCP connection to the EchoLink server,
 * including connection establishment, message transmission/reception, user
 * authentication, and an interactive terminal UI for chat functionality.
 */
class TCPClient {
public:
  /**
   * @brief Constructs a TCPClient instance with server connection parameters.
   *
   * @param ip The IP address of the server to connect to (e.g., "127.0.0.1").
   * @param port The port number on which the server is listening.
   */
  TCPClient(const std::string &ip, int port);

  /**
   * @brief Destructor that ensures proper cleanup of client resources.
   *
   * Calls stop() to close the socket and clean up any active connections.
   */
  ~TCPClient();

  /**
   * @brief Establishes a TCP connection to the remote server.
   *
   * This method:
   * 1. Creates a TCP socket (AF_INET for IPv4, SOCK_STREAM for TCP)
   * 2. Sets the socket to non-blocking mode
   * 3. Initiates a connection to the server address
   * 4. Uses select() with a 5-second timeout to wait for the connection
   * 5. Restores the socket to blocking mode if connection succeeds
   *
   * @return true if connection was successfully established, false otherwise.
   *
   * @note The connection timeout is fixed at 5 seconds.
   * @note On failure, stop() is called to clean up the socket.
   */
  bool connectToServer();

  /**
   * @brief Starts the client's main event loop for interactive communication.
   *
   * This method:
   * 1. Spawns a separate thread to continuously receive messages from the server
   * 2. Initializes the FTXUI interactive screen interface
   * 3. Renders the authentication screen (login/register)
   * 4. Upon successful authentication, switches to the main chat interface
   * 5. Handles user input (messages, commands, tab navigation)
   * 6. Updates the UI with received messages in real-time
   *
   * The run() method blocks until the user exits the application.
   */
  void run();

  /**
   * @brief Gracefully shuts down the client connection and stops all operations.
   *
   * This method:
   * 1. Sets is_running_ to false (signals the receive thread to stop)
   * 2. Closes the socket connection if active
   * 3. Resets socket_fd_ to -1
   *
   * The method is safe to call multiple times as it checks the socket state.
   */
  void stop();

private:
  /**
   * @brief Thread function that continuously receives messages from the server.
   *
   * This method runs in a separate thread and:
   * 1. Continuously receives messages from the server socket
   * 2. Parses authentication responses during the login phase
   * 3. Parses chat messages and special protocol messages (tab creation, etc.)
   * 4. Handles private messages with proper tab management
   * 5. Updates the thread-safe message history
   * 6. Notifies the UI of updates via FTXUI events
   * 7. Terminates when the socket is closed or connection is lost
   *
   * @note This method runs in its own thread and accesses shared data through
   *       history_mutex_ for thread-safe operations.
   */
  void receiveMessages();

  // --- Core Connection Properties ---
  std::string ip_;                ///< Server IP address
  int port_;                      ///< Server port number
  int socket_fd_;                 ///< Socket file descriptor (-1 if not connected)
  std::atomic<bool> is_running_;  ///< Flag indicating if client is active

  // --- Authentication and UI State ---
  std::string username_;                                           ///< Currently logged-in username
  std::string password_;                                           ///< User's password (for authentication)
  std::atomic<bool> is_authenticated_{false};                      ///< Authentication status
  int app_state_ = 0;                                              ///< UI state (0 = login, 1 = chat)
  std::string auth_status_msg_ = "Welcome! Please log in or register."; ///< Status message on login screen

  // --- Chat Tab and Message History Management ---
  std::vector<std::string> chat_tabs_ = {"Global"};               ///< List of available chat tabs
  int selected_tab_ = 0;                                          ///< Currently selected tab index
  std::unordered_map<std::string, std::vector<std::string>> chat_histories_; ///< Message history per tab
  std::mutex history_mutex_;                                      ///< Synchronization mutex for message history

  // --- UI Interface Properties ---
  ftxui::ScreenInteractive* screen_;  ///< Pointer to the FTXUI interactive screen
  int scroll_offset_ = 0;             ///< Current scroll position in message history

  // --- Modal Dialog Properties ---
  bool show_modal_ = false;              ///< Flag to display new chat modal dialog
  std::string new_chat_name_;            ///< Username for new private chat (from modal)
};