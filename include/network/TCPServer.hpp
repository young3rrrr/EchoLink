#pragma once
#include <netinet/in.h>
#include <vector>
#include <mutex>
#include <string>
#include <atomic>

class TCPServer {
public:
    explicit TCPServer(int port);
    ~TCPServer();

    bool start();       // ініціалізація сокету, бінд та початок прослуховування
    void run();         // нескінченний цикл accept()
    void stop();

private:
    // ці функуції теперь методи класу
    void handleClient(int client_socket);
    void broadcastMessage(const std::string& message, int sender_socket);

    int port_;
    int server_fd_;
    sockaddr_in server_address_;
    
    // інкапсуляція даних клієнтів та м'ютекса для синхронізації доступу до них
    std::vector<int> client_sockets_;
    std::mutex clients_mutex_;
    
    bool is_running_; // Додаємо прапорець стану сервера

    void saveMessageToDB(const std::string& username, const std::string& content);
    void sendHistoryToClient(int client_socket);
    
    std::atomic<int> active_threads_{0}; // Лічильник потоків
};