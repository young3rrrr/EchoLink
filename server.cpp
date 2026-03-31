#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define PORT 8080

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        std::cerr << "Failed to bind socket to port " << PORT << "!\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) == -1) {
        std::cerr << "Failed to start listening!\n";
        close(server_fd);
        return 1;
    }

    std::cout << "EchoLink Server started. Waiting for connections on port " << PORT << "...\n";
    
    sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    
    int client_socket = accept(server_fd, (struct sockaddr*)&client_address, &client_len);
    if (client_socket == -1) {
        std::cerr << "Failed to accept client connection!\n";
    } else {
        std::cout << "Success! New client connected!\n";
        close(client_socket); 
    }

    close(server_fd);
    std::cout << "Server stopped...\n";

    return 0;
}