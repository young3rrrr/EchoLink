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

/**
 * Конструктор: Ініціалізує екземпляр TCP клієнта
 * 
 * @param ip - IP адреса сервера, до якого буде підключатись клієнт
 * @param port - номер порту сервера
 * 
 * Деталі реалізації:
 * - Встановлює IP адресу та номер порту сервера
 * - Ініціалізує socket_fd_ значенням -1 (вказує на відсутність активного сокета)
 * - Встановлює is_running_ в false (клієнт не запущений)
 * - Ініціалізує порожнісний рядок для ім'я користувача
 */
TCPClient::TCPClient(const std::string& ip, int port) 
    : ip_(ip), port_(port), socket_fd_(-1), is_running_(false) {}

/**
 * Деструктор: Зчиняє ресурси клієнта
 * 
 * Забезпечує коректне закриття сокета при знищенні об'єкта
 */
TCPClient::~TCPClient() {
    stop();
}

/**
 * connectToServer(): Підключається до TCP сервера
 * 
 * Цей метод виконує наступні кроки:
 * 1. Створює TCP сокет (AF_INET для IPv4, SOCK_STREAM для TCP)
 * 2. Заповнює структуру адреси сервера (IP адреса та порт)
 * 3. Конвертує IP адресу з текстового формату у бінарний
 * 4. Встановлює з'єднання з сервером за допомогою connect()
 * 5. Запитує у користувача введення ім'я (юзернейм)
 * 6. Відправляє на сервер повідомлення про приєднання
 * 7. Встановлює is_running_ в true
 * 
 * @return true якщо з'єднання встановлене успішно, false якщо виникла помилка
 *         При помилці метод виводить повідомлення про помилку в stderr
 */
bool TCPClient::connectToServer() {
    // Створюємо TCP сокет для IPv4
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    // Перевіряємо, чи успішно створено сокет
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

    // Виводимо інформацію про спробу підключення
    std::cout << "Connecting to EchoLink server (" << ip_ << ":" << port_ << ")...\n";

    // Встановлюємо з'єднання з сервером
    if (connect(socket_fd_, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        std::cerr << "Failed to connect to server! Make sure the server is running.\n";
        stop();
        return false;
    }

    // Виводимо повідомлення про успішне з'єднання
    std::cout << "Success! Connection with server established.\n";

    // Запитуємо у користувача введення ім'я (юзернейм) одразу після успішного підключення
    std::cout << "Enter your username: ";
    std::getline(std::cin, username_);

    // Готуємо повідомлення про приєднання користувача до чату
    std::string join_msg = "[Server]: User " + username_ + " joined the chat!";
    // Відправляємо повідомлення про приєднання на сервер
    send(socket_fd_, join_msg.c_str(), join_msg.length(), 0);

    // Відмічаємо, що клієнт запущений і готовий до роботи
    is_running_ = true;
    return true;
}

/**
 * receiveMessages(): Отримує повідомлення від сервера
 * 
 * Цей метод запускається в окремому потоці і виконує:
 * 1. Безперервно читає повідомлення від сервера
 * 2. Виводить отримані повідомлення на екран (консоль)
 * 3. При розриві з'єднання встановлює is_running_ в false
 * 
 * Метод працює в цикл, поки is_running_ не стане false.
 * Якщо recv() повернує значення <= 0, це означає розрив з'єднання.
 */
void TCPClient::receiveMessages() {
    char buffer[1024];  // Буфер для збереження отриманих даних від сервера
    
    // Безперервний цикл отримання повідомлень від сервера
    while (is_running_) {
        // Очищаємо буфер перед отриманням нових даних
        memset(buffer, 0, sizeof(buffer));
        // Отримуємо дані від сервера (блокуючий виклик)
        int bytes_received = recv(socket_fd_, buffer, sizeof(buffer), 0);
        
        // Перевіряємо, чи сервер розірвав з'єднання
        // bytes_received <= 0 означає, що сервер закрив з'єднання
        if (bytes_received <= 0) {
            std::cout << "\n[Connection to server lost]\n";
            // Встановлюємо флаг, що клієнт вже не працює
            is_running_ = false;
            break;
        }
        
        // Виводимо отримане повідомлення на екран
        std::cout << buffer << "\n";
    }
}

/**
 * run(): Основний цикл роботи клієнта
 * 
 * Цей метод:
 * 1. Запускає окремий поток для отримання повідомлень від сервера
 * 2. Циклічно читає введення користувача зі стандартного потоку вводу
 * 3. Обробляє спеціальні команди (наприклад /exit)
 * 4. Форматує та відправляє повідомлення на сервер
 * 5. Завершує роботу при вводі команди /exit або розриві з'єднання
 * 
 * Примітка: Метод використовує std::detach() для запуску потоку отримання
 *           повідомлень незалежно від основного цикла.
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
            // Відправляємо повідомлення про відхід на сервер
            send(socket_fd_, leave_msg.c_str(), leave_msg.length(), 0);
            // Зупиняємо клієнт та закриваємо з'єднання
            stop();
            break;
        }
        
        // Форматуємо повідомлення з ім'ям користувача
        std::string formatted_message = "[" + username_ + "]: " + message;
        // Відправляємо форматоване повідомлення на сервер
        send(socket_fd_, formatted_message.c_str(), formatted_message.length(), 0);
    }
    
    // Виводимо повідомлення про завершення роботи клієнта
    std::cout << "Client finished working.\n";
}

/**
 * stop(): Зупиняє клієнт та закриває з'єднання
 * 
 * Цей метод:
 * 1. Встановлює is_running_ в false, щоб сигналізувати про завершення роботи
 * 2. Перевіряє, чи сокет все ще відкритий (socket_fd_ != -1)
 * 3. Закриває сокет за допомогою close()
 * 4. Встановлює socket_fd_ в -1, щоб позначити закритий стан
 * 
 * Безпечний для повторного виклику - не робить нічого, якщо сокет уже закритий
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