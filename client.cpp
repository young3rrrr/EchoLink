#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"

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

    // TODO: Add code for sending and receiving data; for now, just close the socket

    close(client_socket);
    std::cout << "Client finished working.\n";

    return 0;
}