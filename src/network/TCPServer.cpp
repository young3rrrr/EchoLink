/**
 * @file TCPServer.cpp
 * @brief Реалізація багатопотокового TCP сервера для додатка EchoLink
 * 
 * Цей файл містить реалізацію TCP сервера, який:
 * - Приймає підключення від декількох клієнтів на зазначеному порту
 * - Обробляє вхідні повідомлення від клієнтів
 * - Розповсюджує повідомлення всім підключеним клієнтам
 * - Управляє підключеннями та відключеннями клієнтів
 * 
 * Сервер використовує POSIX socket API та C++ потоки для обробки клієнтів паралельно.
 */

#include "network/TCPServer.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <algorithm>
#include "network/NetworkUtils.h"

/**
 * Конструктор: Ініціалізує екземпляр TCP сервера
 * 
 * @param port - номер порту, на якому сервер буде слухати вхідні з'єднання
 * 
 * Деталі реалізації:
 * - Встановлює номер порту для сервера
 * - Ініціалізує server_fd_ значенням -1 (вказує на відсутність активного сокета)
 * - Очищає структуру server_address_
 * - Налаштовує структуру адреси для IPv4 (AF_INET)
 * - Встановлює INADDR_ANY для прийому з'єднань з будь-якого інтерфейсу
 * - Перетворює номер порту у мережевий формат за допомогою htons()
 */
TCPServer::TCPServer(int port) : port_(port), server_fd_(-1), is_running_(false) { // Додали is_running_(false)
    memset(&server_address_, 0, sizeof(server_address_));
    server_address_.sin_family = AF_INET;
    server_address_.sin_addr.s_addr = INADDR_ANY;
    server_address_.sin_port = htons(port_);
}

/**
 * Деструктор: Звільняє ресурси сервера
 * 
 * Забезпечує коректне закриття сокета сервера при знищенні об'єкта
 */
TCPServer::~TCPServer() {
    stop();
}

/**
 * start(): Ініціалізує та запускає TCP сервер
 * 
 * Цей метод виконує наступні кроки:
 * 1. Створює TCP сокет (AF_INET для IPv4, SOCK_STREAM для TCP)
 * 2. Встановлює опцію SO_REUSEADDR для можливості прив'язки до порту в стані TIME_WAIT
 * 3. Прив'язує сокет до зазначеного порту
 * 4. Починає прослуховувати вхідні з'єднання клієнтів
 * 5. Виводить статусні повідомлення на консоль
 * 
 * @return true якщо сервер успішно запущений, false якщо на якомусь етапі виникла помилка
 *         При помилці метод виводить повідомлення про помилку в stderr
 */
bool TCPServer::start() {
    // Створюємо TCP сокет для IPv4
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    // Перевіряємо, чи успішно створено сокет
    if (server_fd_ == -1) {
        std::cerr << "Error: Failed to create socket.\n";
        return false;
    }

    // Встановлюємо опцію SO_REUSEADDR, щоб дозволити повторне використання порту
    // Це корисно для швидкого перезапуску сервера без очікування завершення TIME_WAIT
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Прив'язуємо сокет до адреси та порту сервера
    if (bind(server_fd_, (struct sockaddr*)&server_address_, sizeof(server_address_)) == -1) {
        // Помилка: порт уже використовується іншим процесом
        std::cerr << "Error: Port " << port_ << " is already in use!\n";
        stop();
        return false;
    }

    // Починаємо прослуховувати вхідні з'єднання
    // SOMAXCONN - максимальна кількість очікуючих з'єднань в черзі
    if (listen(server_fd_, SOMAXCONN) == -1) {
        // Помилка при спробі почати прослуховування
        std::cerr << "Error: cant start listening\n";
        stop();
        return false;
    }

    // Виводимо повідомлення про успішний запуск сервера
    std::cout << "=== EchoLink server started ===\n";
    std::cout << "Listening on port " << port_ << "...\n";
    return true;
}

/**
 * run(): Основний цикл сервера, який приймає та обробляє клієнтські з'єднання
 * 
 * Цей метод:
 * 1. Безперервно чекає на вхідні клієнтські з'єднання за допомогою accept()
 * 2. Створює новий сокет для кожного підключеного клієнта
 * 3. Зберігає сокет клієнта у списку (з захистом мьютексом)
 * 4. Запускає новий потік для обробки повідомлень клієнта
 * 
 * Примітка: Цей метод працює нескінченно і повинен викликатися в окремому потоці.
 *           Метод використовує std::detach() для незалежного запуску обробника для кожного клієнта.
 */
void TCPServer::run() {
    is_running_ = true;

    // Запускаємо окремий потік для читання команд адміністратора з консолі
    std::thread([this]() {
        std::string command;
        while (is_running_) {
            std::getline(std::cin, command);
            
            if (command == "/stop" || command == "exit") {
                std::cout << "[Server] Shutting down...\n";
                stop(); // Викликаємо зупинку
                break;
            } else if (!command.empty()) {
                std::cout << "[Server] Unknown command. Use '/stop' or 'exit' to shutdown.\n";
            }
        }
    }).detach();

    // Основний цикл для постійного прийому клієнтських з'єднань
    while (is_running_) {
        sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        
        // Приймаємо з'єднання
        int client_socket = accept(server_fd_, (struct sockaddr*)&client_address, &client_len);
        
        if (client_socket == -1) {
            // Якщо accept повернув помилку, перевіряємо, чи не зупинили ми сервер
            if (!is_running_) {
                break; // Виходимо з циклу, бо сервер вимикається
            }
            std::cerr << "[Error] Failed to accept client.\n";
            continue;
        }
        
        std::cout << "[Info] New connection! Socket allocated: " << client_socket << "\n";
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_sockets_.push_back(client_socket);
        }
        
        std::thread(&TCPServer::handleClient, this, client_socket).detach();
    }
    
    std::cout << "=== Server successfully stopped ===\n";
}

/**
 * broadcastMessage(): Розповсюджує повідомлення всім підключеним клієнтам
 * 
 * Цей метод відправляє одержане повідомлення всім клієнтам,
 * крім того, хто його надіслав (sender_socket).
 * 
 * @param message - текст повідомлення для розповсюдження
 * @param sender_socket - дескриптор сокета клієнта, який надіслав повідомлення
 *                        цей клієнт не отримає своє ж повідомлення
 */
void TCPServer::broadcastMessage(const std::string& message, int sender_socket) {
    // Захищаємо доступ до списку клієнтів мьютексом
    std::lock_guard<std::mutex> lock(clients_mutex_); 
    
    // Проходимо по всіх підключених клієнтах
    for (int client_fd : client_sockets_) {
        // Не відправляємо повідомлення тому, хто його надіслав
        if (client_fd != sender_socket) {
            // ВАЖЛИВО: Використовуємо нашу нову функцію для безпечної відправки
            sendMessage(client_fd, message);
        }
    }
}

/**
 * handleClient(): Обробляє повідомлення від окремого клієнта
 * 
 * Цей метод (запускається в окремому потоці для кожного клієнта):
 * 1. Циклічно читає повідомлення від клієнта
 * 2. При отриманні повідомлення розповсюджує його всім іншим клієнтам
 * 3. При розриві з'єднання видаляє клієнта зі списку та повідомляє інших
 * 
 * @param client_socket - дескриптор сокета для цього клієнта
 */
void TCPServer::handleClient(int client_socket) {
    
    // Нескінченний цикл обробки повідомлень від клієнта
    while (true) {
        std::string received_msg; // Сюди буде записано повідомлення будь-якого розміру
        
        // Отримуємо дані від клієнта за допомогою нашої нової функції
        // Якщо receiveMessage повертає false, це означає розрив з'єднання або помилку
        if (!receiveMessage(client_socket, received_msg)) {
            // Логуємо відключення клієнта
            std::cout << "[Info] Client " << client_socket << " disconnected.\n";
            
            // Видаляємо клієнта зі списку підключених (з захистом мьютексом)
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_sockets_.erase(std::remove(client_sockets_.begin(), client_sockets_.end(), client_socket), client_sockets_.end());
            close(client_socket);
            
            // Повідомляємо інших клієнтів про розрив з'єднання
            std::string disconnect_msg = "[Server]: Someone left the chat.";
            for (int fd : client_sockets_) {
                // ВАЖЛИВО: Замінили send(...) на нашу нову функцію!
                sendMessage(fd, disconnect_msg);
            }
            break; // Вихід з циклу
        }
        
        // Ця перевірка залишається без змін
        if (received_msg.find_first_not_of(" \t\n\r\f\v") == std::string::npos) {
            // Якщо повідомлення містить лише пробіли, пропускаємо його
            continue;
        }

        // Логуємо отримане повідомлення
        std::cout << "[server log] received: " << received_msg << "\n";
        
        // Розповсюджуємо повідомлення всім іншим клієнтам
        broadcastMessage(received_msg, client_socket);
    }
}

/**
 * stop(): Зупиняє сервер та закриває сокет
 * 
 * Цей метод:
 * 1. Перевіряє, чи сокет відкритий (server_fd_ != -1)
 * 2. Закриває сокет за допомогою close()
 * 3. Встановлює server_fd_ в -1, щоб позначити закритий стан
 * 
 * Безпечний для повторного виклику - не робить нічого, якщо сокет уже закритий
 */
void TCPServer::stop() {
    is_running_ = false; // Сигналізуємо всім циклам про зупинку

    // Сповіщаємо та відключаємо всіх підключених клієнтів
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::string shutdown_msg = "[Server]: Server is shutting down.";
        for (int fd : client_sockets_) {
            sendMessage(fd, shutdown_msg); // Сповіщаємо клієнта
            close(fd);                     // Закриваємо його сокет
        }
        client_sockets_.clear(); // Очищаємо список
    }

// Перевіряємо, чи сокет сервера ще відкритий
    if (server_fd_ != -1) {
        // === ДОДАНО ЦЮ СТРОЧКУ ===
        // Примусово перериваємо всі операції читання/запису, що розблокує accept()
        shutdown(server_fd_, SHUT_RDWR); 
        
        close(server_fd_); // Тепер безпечно закриваємо
        server_fd_ = -1;
    }
}