/**
 * @file TCPClient.cpp
 * @brief Реалізація TCP клієнта для додатку EchoLink
 * 
 * Цей файл містить реалізацію TCP клієнта, який:
 * - Підключається до TCP сервера на вказаній IP адресі та порту
 * - Отримує повідомлення від сервера та інших клієнтів
 * - Відправляє повідомлення на сервер
 * - Управління з'єднанням та розривом з'єднання
 * 
 * Клієнт використовує POSIX socket API та C++ потоки для одночасної
 * обробки отримання та відправлення повідомлень.
 */

#include "network/TCPClient.hpp"

#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "network/NetworkUtils.h" // Наш новий файл з функціями для динамічного буфера
#include <fcntl.h> // Додаємо для неблокуючого сокета
#include <sys/select.h>

/**
 * Конструктор: Ініціалізує екземпляр TCP клієнта
 */
TCPClient::TCPClient(const std::string& ip, int port) 
    : ip_(ip), port_(port), socket_fd_(-1), is_running_(false) {}

/**
 * Деструктор: Зчиняє ресурси клієнта
 */
TCPClient::~TCPClient() {
    stop();
}

/**
 * connectToServer(): Підключається до TCP сервера
 */
bool TCPClient::connectToServer() {
    // Створюємо TCP сокет для IPv4
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ == -1) {
        std::cerr << "Failed to create client socket\n";
        return false;
    }

    // Заповнюємо структуру адреси сервера
    sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;                    // IPv4
    server_address.sin_port = htons(port_);                 // Перетворюємо номер порту

    // Конвертуємо IP адресу з текстового формату у бінарний
    if (inet_pton(AF_INET, ip_.c_str(), &server_address.sin_addr) <= 0) {
        std::cerr << "Failed to convert IP address\n";
        stop();
        return false;
    }

    std::cout << "Connecting to EchoLink server (" << ip_ << ":" << port_ << ")...\n";

    // 1. Отримуємо поточні прапорці сокета і додаємо режим O_NONBLOCK (неблокуючий)
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

    // 2. Робимо спробу підключення (вона поверне керування миттєво)
    int res = connect(socket_fd_, (struct sockaddr*)&server_address, sizeof(server_address));
    
    if (res < 0) {
        if (errno == EINPROGRESS) {
            // Підключення в процесі. Використовуємо select для очікування з таймаутом
            fd_set write_set;
            FD_ZERO(&write_set);
            FD_SET(socket_fd_, &write_set);

            struct timeval timeout;
            timeout.tv_sec = 5;  // Максимальний час очікування (5 секунд)
            timeout.tv_usec = 0; // Мікросекунди

            // Чекаємо, поки сокет стане доступним для запису (що означає успішне підключення) або вийде час
            res = select(socket_fd_ + 1, NULL, &write_set, NULL, &timeout);
            
            if (res == 0) {
                // Таймаут! Сервер не відповів за 5 секунд
                std::cerr << "Connection timed out! The server is unreachable.\n";
                stop();
                return false;
            } else if (res > 0) {
                // Перевіряємо, чи немає прихованих помилок на сокеті
                int so_error;
                socklen_t len = sizeof(so_error);
                getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &so_error, &len);
                if (so_error != 0) {
                    std::cerr << "Connection failed.\n";
                    stop();
                    return false;
                }
            } else {
                std::cerr << "Select error during connection.\n";
                stop();
                return false;
            }
        } else {
            std::cerr << "Failed to connect to server immediately!\n";
            stop();
            return false;
        }
    }

    // 3. Повертаємо сокет назад у нормальний (блокуючий) режим для подальшої роботи
    fcntl(socket_fd_, F_SETFL, flags);

    std::cout << "Success! Connection with server established.\n";

    // Запитуємо у користувача введення ім'я (юзернейм) одразу після успішного підключення
    std::cout << "Enter your username: ";
    std::getline(std::cin, username_);

    // Готуємо повідомлення про приєднання користувача до чату
    std::string join_msg = "[Server]: User " + username_ + " joined the chat!";
    
    // ВАЖЛИВО: Використовуємо нашу нову функцію замість send()
    sendMessage(socket_fd_, join_msg);

    // Відмічаємо, що клієнт запущений і готовий до роботи
    is_running_ = true;
    return true;
}

/**
 * receiveMessages(): Отримує повідомлення від сервера
 */
void TCPClient::receiveMessages() {
    // Буфер char[1024] більше не потрібен!
    
    // Безперервний цикл отримання повідомлень від сервера
    while (is_running_) {
        std::string received_msg; // Сюди буде записано повідомлення будь-якого розміру
        
        // Отримуємо дані від сервера за допомогою нашої нової функції
        // Якщо receiveMessage повертає false, це означає розрив з'єднання
        if (!receiveMessage(socket_fd_, received_msg)) {
            std::cout << "\n[Connection to server lost]\n";
            // Встановлюємо флаг, що клієнт вже не працює
            is_running_ = false;
            break; // Вихід з циклу
        }
        
        // Виводимо отримане повідомлення на екран
        std::cout << received_msg << "\n";
    }
}

/**
 * run(): Основний цикл роботи клієнта
 */
void TCPClient::run() {
    // Запускаємо окремий поток для прослухування вхідних повідомлень від сервера
    std::thread(&TCPClient::receiveMessages, this).detach();

    std::string message;
    // Основний цикл для отримання введення від користувача
    while (is_running_) {
        // Читаємо рядок, введений користувачем
        std::getline(std::cin, message);
        
        message.erase(0, message.find_first_not_of(" \t\n\r\f\v")); // Видаляємо початкові пробіли
        message.erase(message.find_last_not_of(" \t\n\r\f\v") + 1); // Видаляємо кінцеві пробіли

        if (message.empty()) {
            // Якщо користувач ввів лише пробіли, пропускаємо відправку порожнього повідомлення
            continue;
        }

        // Якщо сервер відключився і поток отримання завершився, виходимо з циклу вводу
        if (!is_running_) break; 

        // Перевіряємо спеціальну команду для виходу
        if (message == "/exit") {
            // Готуємо повідомлення про відхід користувача
            std::string leave_msg = "[Server]: User " + username_ + " left the chat.";
            
            // ВАЖЛИВО: Використовуємо нашу нову функцію замість send()
            sendMessage(socket_fd_, leave_msg);
            
            // Зупиняємо клієнт та закриваємо з'єднання
            stop();
            break;
        }
        
        // Форматуємо повідомлення з ім'ям користувача
        std::string formatted_message = "[" + username_ + "]: " + message;
        
        // ВАЖЛИВО: Використовуємо нашу нову функцію замість send()
        sendMessage(socket_fd_, formatted_message);
    }
    
    // Виводимо повідомлення про завершення роботи клієнта
    std::cout << "Client finished working.\n";
}

/**
 * stop(): Зупиняє клієнт та закриває з'єднання
 */
void TCPClient::stop() {
    // Перевіряємо, чи сокет ще відкритий
    if (socket_fd_ != -1) {
        // Встановлюємо флаг, що клієнт повинен припинити роботу
        is_running_ = false;
        // Закриваємо сокет клієнта
        close(socket_fd_);
        // Позначаємо, що сокет закритий
        socket_fd_ = -1;
    }
}