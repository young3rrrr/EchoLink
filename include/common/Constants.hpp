/**
 * @file Constants.hpp
 * @brief Global constants used throughout the EchoLink application.
 *
 * This header file defines configuration constants for the EchoLink chat
 * application, including default network settings for server connectivity.
 */

#pragma once

/**
 * @namespace echolink
 * @brief Main namespace containing all EchoLink application code.
 *
 * This namespace encapsulates all constants, classes, and functions related
 * to the EchoLink chat application to avoid naming conflicts.
 */
namespace echolink {

/// Default TCP port for the EchoLink server listening for client connections
constexpr int DEFAULT_PORT = 8080;

/// Default IPv4 address of the EchoLink server (localhost for local development)
constexpr const char *DEFAULT_SERVER_IP = "echolink";

} // namespace echolink