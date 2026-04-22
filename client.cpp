#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"

void receive_messages(int sock) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        
        if (bytes_received <= 0) {
            std::cout << "\n[Connection to server lost]\n";
            break;
        }
        
        std::cout << buffer << "\n";
    }
}

int main() {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Failed to create client socket\n";
        return 1;
    }

    sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) <= 0) {
        std::cerr << "Failed to convert IP address\n";
        close(client_socket);
        return 1;
    }

    std::cout << "Connecting to EchoLink server (" << SERVER_IP << ":" << PORT << ")...\n";

    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        std::cerr << "Failed to connect to server! Make sure the server is running.\n";
        close(client_socket);
        return 1;
    }

    std::cout << "Success! Connection with server established.\n";

    std::string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);

    std::string join_msg = "[Server]: User " + username + " joined the chat!";
    send(client_socket, join_msg.c_str(), join_msg.length(), 0);

    std::thread(receive_messages, client_socket).detach();

    std::string message;
    while (true) {
        std::getline(std::cin, message);
        
        if (message == "/exit") {
            std::string leave_msg = "[Server]: User " + username + " left the chat.";
            send(client_socket, leave_msg.c_str(), leave_msg.length(), 0);
            break;
        }
        
        std::string formatted_message = "[" + username + "]: " + message;
        
        send(client_socket, formatted_message.c_str(), formatted_message.length(), 0);
    }


    close(client_socket);
    std::cout << "Client finished working.\n";

    return 0;
}