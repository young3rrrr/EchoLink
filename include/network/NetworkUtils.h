/**
 * @file NetworkUtils.h
 * @brief Network communication utilities for message transmission over TCP sockets.
 *
 * This file provides inline utility functions for reliably sending and receiving
 * messages over TCP sockets using length-prefixed message framing. It implements
 * a custom protocol where each message is preceded by a 4-byte network-byte-order
 * integer indicating the message length, followed by the message data.
 *
 * The implementation handles partial sends and receives to ensure complete message
 * delivery, includes DoS attack prevention through message size limits, and uses
 * network-byte-order (big-endian) for cross-platform compatibility.
 */

#pragma once

#include <string>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>

/**
 * @brief Transmits a message over a TCP socket with length-prefixed framing.
 *
 * This function sends a message by first transmitting a 4-byte header containing
 * the message length (in network-byte-order), followed by the message payload.
 * The implementation handles partial sends by looping until the entire message
 * is transmitted. If any send operation fails, the function returns immediately
 * with failure status.
 *
 * @param socket The file descriptor of the connected TCP socket.
 * @param message The message string to transmit.
 *
 * @return true if the message header and all message data were successfully sent,
 *         false if any send operation failed (socket error or connection closed).
 *
 * @note The message is transmitted in network-byte-order (big-endian) for proper
 *       cross-platform compatibility.
 */
inline bool sendMessage(int socket, const std::string& message) {
    // Convert message size to network-byte-order (big-endian) for cross-platform compatibility
    uint32_t msgSize = message.size();
    uint32_t netSize = htonl(msgSize);

    // Transmit the 4-byte message length header
    if (send(socket, &netSize, sizeof(netSize), 0) == -1) {
        return false;
    }

    // Transmit the message payload, handling partial sends in a loop
    size_t totalSent = 0;
    while (totalSent < msgSize) {
        ssize_t sent = send(socket, message.c_str() + totalSent, msgSize - totalSent, 0);
        if (sent == -1) {
            return false;
        }
        totalSent += sent;
    }
    return true;
}

/**
 * @brief Receives a length-prefixed message from a TCP socket.
 *
 * This function receives a message by first reading a 4-byte header containing
 * the message length (in network-byte-order), then reading the corresponding
 * number of bytes into the output buffer. The function handles partial receives
 * by looping until the complete message is read.
 *
 * The function includes a DoS attack prevention mechanism that rejects messages
 * larger than 10 MB (10,485,760 bytes). If the received message size exceeds
 * this limit, the function returns failure immediately.
 *
 * @param socket The file descriptor of the connected TCP socket.
 * @param outMessage Reference to a string where the received message will be stored.
 *
 * @return true if the complete message (including header and payload) was
 *         successfully received and stored in outMessage, false if the socket
 *         connection was closed, a receive error occurred, or the message size
 *         exceeded the maximum allowed size.
 *
 * @note The function expects messages to be framed with a 4-byte length header
 *       in network-byte-order, as produced by sendMessage().
 * @note Empty messages (0-byte payload) are valid and return true with an
 *       empty string in outMessage.
 */
inline bool receiveMessage(int socket, std::string& outMessage) {
    uint32_t netSize = 0;

    // Receive the 4-byte message length header from the socket
    // MSG_WAITALL ensures we wait until the full header is received
    ssize_t headerBytesRead = recv(socket, &netSize, sizeof(netSize), MSG_WAITALL);
    if (headerBytesRead <= 0) {
        return false;
    }

    // Convert the message length from network-byte-order to host-byte-order
    uint32_t msgSize = ntohl(netSize);

    // Handle empty messages (size == 0)
    if (msgSize == 0) {
        outMessage = "";
        return true;
    }

    // DoS attack prevention: Reject messages larger than 10 MB
    if (msgSize > 10 * 1024 * 1024) {
        return false;
    }

    // Allocate a buffer for the complete message payload
    std::vector<char> buffer(msgSize);
    size_t totalRead = 0;

    // Receive message payload, handling partial reads in a loop
    while (totalRead < msgSize) {
        ssize_t bytesRead = recv(socket, buffer.data() + totalRead, msgSize - totalRead, 0);
        if (bytesRead <= 0) {
            return false;
        }
        totalRead += bytesRead;
    }

    // Convert the received buffer to a string and return success
    outMessage.assign(buffer.begin(), buffer.end());
    return true;
}