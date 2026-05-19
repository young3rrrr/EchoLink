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
#include <algorithm>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

using namespace ftxui;

/**
 * @brief Constructs a TCPClient with specified server connection parameters.
 *
 * Initializes all member variables, setting the socket to an invalid state
 * (socket_fd_ = -1) and marking the client as not running.
 *
 * @param ip The server IP address to connect to.
 * @param port The server port number.
 */
TCPClient::TCPClient(const std::string &ip, int port)
    : ip_(ip), port_(port), socket_fd_(-1), is_running_(false), screen_(nullptr) {}

/**
 * @brief Destructor that closes the connection and stops the client.
 */
TCPClient::~TCPClient() { stop(); }

/**
 * @brief Establishes a TCP connection to the specified server.
 *
 * This method implements non-blocking socket connection with timeout handling:
 * 1. Creates an IPv4 TCP socket
 * 2. Sets the socket to non-blocking mode
 * 3. Initiates connection to the server
 * 4. Uses select() to wait for connection with a 5-second timeout
 * 5. Restores blocking mode on success
 *
 * @return true if connection succeeded, false otherwise.
 */
bool TCPClient::connectToServer() {
  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ == -1) return false;

  sockaddr_in server_address;
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port_);
  inet_pton(AF_INET, ip_.c_str(), &server_address.sin_addr);

  // Set socket to non-blocking mode for timeout-based connection
  int flags = fcntl(socket_fd_, F_GETFL, 0);
  fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

  // Initiate connection (expected to return EINPROGRESS for non-blocking socket)
  int res = connect(socket_fd_, (struct sockaddr *)&server_address, sizeof(server_address));
  if (res < 0 && errno == EINPROGRESS) {
    // Wait for connection to complete with 5-second timeout using select()
    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(socket_fd_, &write_set);
    struct timeval timeout{5, 0};
    res = select(socket_fd_ + 1, NULL, &write_set, NULL, &timeout);
    if (res <= 0) { 
      stop(); 
      return false; 
    }
  }

  // Restore blocking mode for normal socket operations
  fcntl(socket_fd_, F_SETFL, flags);

  is_running_ = true;
  return true;
}

/**
 * @brief Thread function that continuously receives messages from the server.
 *
 * This function:
 * 1. Continuously receives messages from the server socket
 * 2. Parses authentication responses during login phase
 * 3. Handles special protocol messages (system tab creation)
 * 4. Routes private and public messages to appropriate chat tabs
 * 5. Updates the UI with received messages
 * 6. Terminates when the connection is lost
 */
void TCPClient::receiveMessages() {
  while (is_running_) {
    std::string received_msg;

    // Attempt to receive a message from the server
    if (!receiveMessage(socket_fd_, received_msg)) {
      // Connection lost - update UI and stop running
      {
        std::lock_guard<std::mutex> lock(history_mutex_);
        auth_status_msg_ = "[Connection to server lost]";
        chat_histories_["Global"].push_back("[Connection to server lost]");
      }
      is_running_ = false;
      if (screen_) screen_->PostEvent(Event::Custom);
      break;
    }

    // Handle authentication phase responses
    if (!is_authenticated_) {
      std::lock_guard<std::mutex> lock(history_mutex_);
      if (received_msg.find("Login successful!") != std::string::npos) {
        // Authentication succeeded - switch to main chat screen
        is_authenticated_ = true;
        app_state_ = 1;
      } else {
        // Authentication failed - display error message on login screen
        auth_status_msg_ = received_msg;
      }
    } else {
      // Handle messages during authenticated chat session

      // Special system protocol: tab creation message
      if (received_msg.rfind("[SYS_TAB_OPEN]", 0) == 0) {
        std::string target_tab = received_msg.substr(14);

        std::lock_guard<std::mutex> lock(history_mutex_);
        auto it = std::find(chat_tabs_.begin(), chat_tabs_.end(), target_tab);

        if (it == chat_tabs_.end()) {
          // New tab - add it and switch to it
          chat_tabs_.push_back(target_tab);
          selected_tab_ = chat_tabs_.size() - 1;
        } else {
          // Tab already exists - switch to it
          selected_tab_ = std::distance(chat_tabs_.begin(), it);
        }

        scroll_offset_ = 0;
        if (screen_) screen_->PostEvent(Event::Custom);
        continue;  // Skip displaying system message as text
      }

      // Parse message destination (default to global)
      std::string target_tab = "Global";
      std::string display_msg = received_msg;

      // Handle private messages with [PRIVATE_MSG][username] format
      if (received_msg.rfind("[PRIVATE_MSG][", 0) == 0) {
        size_t end_bracket = received_msg.find("]", 14);
        if (end_bracket != std::string::npos) {
          target_tab = received_msg.substr(14, end_bracket - 14);
          display_msg = received_msg.substr(end_bracket + 1);

          std::lock_guard<std::mutex> lock(history_mutex_);
          // Create private chat tab if it doesn't exist
          if (std::find(chat_tabs_.begin(), chat_tabs_.end(), target_tab) == chat_tabs_.end()) {
            chat_tabs_.push_back(target_tab);
          }
        }
      }

      // Store message in the appropriate chat history
      {
        std::lock_guard<std::mutex> lock(history_mutex_);
        chat_histories_[target_tab].push_back(display_msg);
      }
    }

    // Notify UI of changes
    if (screen_) screen_->PostEvent(Event::Custom);
  }
}

/**
 * @brief Starts the client's main interactive user interface loop.
 *
 * This method:
 * 1. Spawns a receive thread to handle incoming messages
 * 2. Creates the FTXUI interactive screen
 * 3. Builds authentication UI (login/register screen)
 * 4. Builds chat UI with tabs and message history
 * 5. Implements keyboard shortcuts for navigation
 * 6. Handles user commands (/exit, /stop)
 * 7. Runs the event loop until the user exits
 */
void TCPClient::run() {
  // Spawn receive thread for background message handling
  std::thread(&TCPClient::receiveMessages, this).detach();

  auto screen = ScreenInteractive::Fullscreen();
  screen_ = &screen;

  // =========================================================
  // UI SCREEN 0: AUTHENTICATION (LOGIN/REGISTER)
  // =========================================================
  InputOption pass_opt;
  pass_opt.password = true;  // Hide password input
  Component input_user = Input(&username_, "Username");
  Component input_pass = Input(&password_, "Password", pass_opt);

  Component btn_login = Button(" Login ", [&] {
    if (!username_.empty() && !password_.empty())
      sendMessage(socket_fd_, "/login " + username_ + " " + password_);
  });
  Component btn_reg = Button(" Register ", [&] {
    if (!username_.empty() && !password_.empty())
      sendMessage(socket_fd_, "/register " + username_ + " " + password_);
  });

  Component auth_container = Container::Vertical({
    input_user, input_pass, Container::Horizontal({btn_login, btn_reg})
  });

  auto auth_renderer = Renderer(auth_container, [&] {
    return window(text(" EchoLink Authentication ") | bold, vbox({
        text(auth_status_msg_) | color(Color::Yellow),
        separator(),
        hbox(text(" Login: "), input_user->Render()),
        hbox(text(" Pass:  "), input_pass->Render()),
        separator(),
        hbox(btn_login->Render(), text("  "), btn_reg->Render()) | center
    })) | center;
  });

  // =========================================================
  // MODAL DIALOG: NEW PRIVATE CHAT
  // =========================================================
  Component modal_input = Input(&new_chat_name_, "Username...");
  Component modal_btn_ok = Button("Start Chat", [&] {
    if (!new_chat_name_.empty()) {
      // Request server to validate user and create private chat
      sendMessage(socket_fd_, "/check_user " + new_chat_name_);
    }
    show_modal_ = false;
    new_chat_name_.clear();
  });

  Component modal_btn_cancel = Button("Cancel", [&] {
    show_modal_ = false;
    new_chat_name_.clear();
  });

  Component modal_container = Container::Vertical({
    modal_input,
    Container::Horizontal({modal_btn_ok, modal_btn_cancel})
  });

  auto modal_renderer = Renderer(modal_container, [&] {
    return window(text(" New Private Chat "), vbox({
        text("Enter recipient's username:"),
        modal_input->Render(),
        separator(),
        hbox({modal_btn_ok->Render(), text(" "), modal_btn_cancel->Render()}) | center
    })) | clear_under | center;
  });

  // =========================================================
  // UI SCREEN 1: MAIN CHAT INTERFACE
  // =========================================================
  std::string input_text;
  InputOption chat_opt;
  chat_opt.on_enter = [&] {
    if (input_text.empty()) return;

    // Handle special exit commands
    if (input_text == "/exit" || input_text == "/stop") {
      sendMessage(socket_fd_, "[Server]: User " + username_ + " left the chat.");
      stop();
      screen.Exit();
      return;
    }

    // Send message to current chat tab
    std::string current_tab = chat_tabs_[selected_tab_];
    if (current_tab == "Global") {
      // Send to global chat
      sendMessage(socket_fd_, "[" + username_ + "]: " + input_text);
    } else {
      // Send as private message
      sendMessage(socket_fd_, "/msg " + current_tab + " " + input_text);
    }

    // Add message to local history for global chat only
    if (current_tab == "Global") {
      std::lock_guard<std::mutex> lock(history_mutex_);
      chat_histories_["Global"].push_back("[" + username_ + "]: " + input_text);
    }

    scroll_offset_ = 0;
    input_text.clear();
  };

  Component input_field = Input(&input_text, "Type message...", chat_opt);
  Component btn_new_chat = Button("[+ New Chat]", [&] { show_modal_ = true; });

  MenuOption menu_option;
  menu_option.on_change = [&] { scroll_offset_ = 0; };
  Component menu = Menu(&chat_tabs_, &selected_tab_, menu_option);

  Component left_panel = Container::Vertical({ btn_new_chat, menu });
  Component chat_container = Container::Horizontal({ left_panel, input_field });

  // Implement keyboard navigation for message scrolling
  chat_container = CatchEvent(chat_container, [&](Event event) {
    if (event == Event::ArrowUp || event == Event::PageUp) {
      scroll_offset_ += (event == Event::PageUp) ? 10 : 1;
      return true;
    }
    if (event == Event::ArrowDown || event == Event::PageDown) {
      scroll_offset_ -= (event == Event::PageDown) ? 10 : 1;
      if (scroll_offset_ < 0) scroll_offset_ = 0;
      return true;
    }
    return false;
  });

  auto chat_renderer = Renderer(chat_container, [&] {
    Elements history_elements;
    std::string current_tab;

    // Fetch current chat history with thread safety
    {
      std::lock_guard<std::mutex> lock(history_mutex_);
      current_tab = chat_tabs_[selected_tab_];
      const auto& msgs = chat_histories_[current_tab];

      // Bounds check for scroll offset
      if (scroll_offset_ < 0) scroll_offset_ = 0;
      if (!msgs.empty() && scroll_offset_ >= (int)msgs.size()) {
        scroll_offset_ = msgs.size() - 1;
      }

      // Build message elements for rendering
      for (int i = 0; i < (int)msgs.size(); ++i) {
        auto el = text(msgs[i]);
        if (i == (int)msgs.size() - 1 - scroll_offset_) {
          el = el | focus;
        }
        history_elements.push_back(el);
      }
    }

    auto history_box = vbox(std::move(history_elements)) | yframe | yflex;

    // Render the complete chat interface
    return hbox({
      vbox({
        text(" MENU ") | bold | center,
        separator(),
        btn_new_chat->Render() | center,
        separator(),
        menu->Render() | yframe | yflex
      }) | border | size(WIDTH, LESS_THAN, 25),
      vbox({
        text(" EchoLink: " + current_tab + " ") | bold | center,
        separator(),
        history_box,
        separator(),
        hbox({text(" > ") | color(Color::Green), input_field->Render()})
      }) | border | flex
    });
  });

  // Add modal dialog overlay to chat screen
  chat_renderer = Modal(chat_renderer, modal_renderer, &show_modal_);

  // Create tab-based interface to switch between authentication and chat screens
  auto root_container = Container::Tab({auth_renderer, chat_renderer}, &app_state_);
  auto root_renderer = Renderer(root_container, [&] { return root_container->Render(); });

  // Run the event loop
  screen.Loop(root_renderer);
  screen_ = nullptr;
}

/**
 * @brief Stops the client and closes the network connection.
 *
 * This method:
 * 1. Sets is_running_ to false (signals receive thread to stop)
 * 2. Closes the socket if it's valid
 * 3. Resets socket_fd_ to -1
 *
 * Safe to call multiple times.
 */
void TCPClient::stop() {
  if (socket_fd_ != -1) {
    is_running_ = false;
    close(socket_fd_);
    socket_fd_ = -1;
  }
}