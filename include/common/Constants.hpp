#pragma once
namespace echolink {

/// Default TCP port for the EchoLink server listening for client connections
constexpr int DEFAULT_PORT = 8080;

/// Default IPv4 address of the EchoLink server (echolink for containerized environments)
constexpr const char *DEFAULT_SERVER_IP = "echolink";

}