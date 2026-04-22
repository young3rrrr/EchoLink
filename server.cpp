#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080

std::vector<int> client_sockets;
std::mutex clients_mutex;

void broadcast_message(const std::string& message, int sender_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex); 
    
    for (int client_fd : client_sockets) {
        if (client_fd != sender_socket) {
            send(client_fd, message.c_str(), message.length(), 0);
        }
    }
}

void handle_client(int client_socket) {
    char buffer[1024];
    
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        
        if (bytes_received <= 0) {
            std::cout << "[Info] Client " << client_socket << " disconnected.\n";
            
            std::lock_guard<std::mutex> lock(clients_mutex);
            client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), client_socket), client_sockets.end());
            close(client_socket);
            
            std::string disconnect_msg = "[Server]: Someone left the chat.";
            for (int fd : client_sockets) {
                send(fd, disconnect_msg.c_str(), disconnect_msg.length(), 0);
            }
            break;
        }
        
        std::string received_msg(buffer);
        std::cout << "[server log] received: " << received_msg << "\n";
        
        broadcast_message(received_msg, client_socket);
    }
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Error: Failed to create socket.\n";
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        std::cerr << "Error: Port " << PORT << " is already in use!\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) == -1) {
        std::cerr << "Error: cant start listening\n";
        close(server_fd);
        return 1;
    }

    std::cout << "=== EchoLink server started ===\n";
    std::cout << "Listening on port" << PORT << "...\n";

    while (true) {
        sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        
        int client_socket = accept(server_fd, (struct sockaddr*)&client_address, &client_len);
        if (client_socket == -1) {
            std::cerr << "[Error] Failed to accept client.\n";
            continue;
        }
        
        std::cout << "[Info] New connection! Socket allocated: " << client_socket << "\n";
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            client_sockets.push_back(client_socket);
        }
        
        std::thread(handle_client, client_socket).detach();
    }

    close(server_fd);
    return 0;
}