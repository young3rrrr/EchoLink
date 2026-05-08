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
TCPServer::TCPServer(int port) : port_(port), server_fd_(-1) {
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
    // Нескінченний цикл для постійного прийому клієнтських з'єднань
    while (true) {
        sockaddr_in client_address;           // Структура для збереження адреси клієнта
        socklen_t client_len = sizeof(client_address);
        
        // Приймаємо з'єднання від клієнта (блокуючий виклик)
        // Цей виклик чекає на наступне вхідне з'єднання
        int client_socket = accept(server_fd_, (struct sockaddr*)&client_address, &client_len);
        // Перевіряємо, чи успішно прийняли з'єднання
        if (client_socket == -1) {
            std::cerr << "[Error] Failed to accept client.\n";
            continue;
        }
        
        // Логуємо нове з'єднання з дескриптором файлу сокета
        std::cout << "[Info] New connection! Socket allocated: " << client_socket << "\n";
        
        // Додаємо новий сокет клієнта до списку підключених клієнтів
        // Мьютекс забезпечує потокобезпечний доступ до списку client_sockets_
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_sockets_.push_back(client_socket);
        }
        
        // Створюємо новий потік для обробки повідомлень цього клієнта
        // std::detach() дозволяє потоку працювати незалежно від основного циклу сервера
        std::thread(&TCPServer::handleClient, this, client_socket).detach();
    }
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
            // Відправляємо повідомлення до клієнта
            send(client_fd, message.c_str(), message.length(), 0);
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
    char buffer[1024];  // Буфер для зберігання вхідних даних від клієнта
    
    // Нескінченний цикл обробки повідомлень від клієнта
    while (true) {
        // Очищаємо буфер перед отриманням нових даних
        memset(buffer, 0, sizeof(buffer));
        // Отримуємо дані від клієнта
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        
        // Перевіряємо, чи сталося розривання з'єднання
        // bytes_received <= 0 означає, що клієнт відключився
        if (bytes_received <= 0) {
            // Логуємо відключення клієнта
            std::cout << "[Info] Client " << client_socket << " disconnected.\n";
            
            // Видаляємо клієнта зі списку підключених (з захистом мьютексом)
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_sockets_.erase(std::remove(client_sockets_.begin(), client_sockets_.end(), client_socket), client_sockets_.end());
            close(client_socket);
            
            // Повідомляємо інших клієнтів про розрив з'єднання
            std::string disconnect_msg = "[Server]: Someone left the chat.";
            for (int fd : client_sockets_) {
                send(fd, disconnect_msg.c_str(), disconnect_msg.length(), 0);
            }
            break;
        }
        
        // Перетворюємо отримані дані в рядок
        std::string received_msg(buffer);
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
    // Перевіряємо, чи сокет ще відкритий
    if (server_fd_ != -1) {
        // Закриваємо сокет сервера
        close(server_fd_);
        // Позначаємо, що сокет закритий
        server_fd_ = -1;
    }
}