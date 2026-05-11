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
 * Сервер використовує POSIX socket API та C++ потоки для обробки клієнтів
 * паралельно.
 */

#include "network/TCPServer.hpp"
#include "network/NetworkUtils.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <pqxx/pqxx>
#include <thread>
#include <unistd.h>

// Рядок підключення до бази даних
const std::string DB_CONN = "dbname=echolink_db user=echolink_user "
                            "password=14341225 hostaddr=127.0.0.1 port=5432";

/**
 * Конструктор: Ініціалізує екземпляр TCP сервера
 *
 * @param port - номер порту, на якому сервер буде слухати вхідні з'єднання
 *
 * Деталі реалізації:
 * - Встановлює номер порту для сервера
 * - Ініціалізує server_fd_ значенням -1 (вказує на відсутність активного
 * сокета)
 * - Очищає структуру server_address_
 * - Налаштовує структуру адреси для IPv4 (AF_INET)
 * - Встановлює INADDR_ANY для прийому з'єднань з будь-якого інтерфейсу
 * - Перетворює номер порту у мережевий формат за допомогою htons()
 */
TCPServer::TCPServer(int port)
    : port_(port), server_fd_(-1),
      is_running_(false) { // Додали is_running_(false)
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
TCPServer::~TCPServer() { stop(); }

/**
 * start(): Ініціалізує та запускає TCP сервер
 *
 * Цей метод виконує наступні кроки:
 * 1. Створює TCP сокет (AF_INET для IPv4, SOCK_STREAM для TCP)
 * 2. Встановлює опцію SO_REUSEADDR для можливості прив'язки до порту в стані
 * TIME_WAIT
 * 3. Прив'язує сокет до зазначеного порту
 * 4. Починає прослуховувати вхідні з'єднання клієнтів
 * 5. Виводить статусні повідомлення на консоль
 *
 * @return true якщо сервер успішно запущений, false якщо на якомусь етапі
 * виникла помилка При помилці метод виводить повідомлення про помилку в stderr
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
  // Це корисно для швидкого перезапуску сервера без очікування завершення
  // TIME_WAIT
  int opt = 1;
  setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // Прив'язуємо сокет до адреси та порту сервера
  if (bind(server_fd_, (struct sockaddr *)&server_address_,
           sizeof(server_address_)) == -1) {
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
 * Примітка: Цей метод працює нескінченно і повинен викликатися в окремому
 * потоці. Метод використовує std::detach() для незалежного запуску обробника
 * для кожного клієнта.
 */
void TCPServer::run() {
  is_running_ = true;

  std::vector<std::thread> client_threads;

  std::thread console_thread([this]() {
    std::string command;
    while (is_running_) {
      std::getline(std::cin, command);

      if (command == "/stop" || command == "exit") {
        std::cout << "[Server] Shutting down...\n";
        stop();
        break;
      } else if (!command.empty()) {
        std::cout << "[Server] Unknown command.\n";
      }
    }
  });

  // Основний цикл для прийому з'єднань
  while (is_running_) {
    sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);

    int client_socket =
        accept(server_fd_, (struct sockaddr *)&client_address, &client_len);

    if (client_socket == -1) {
      if (!is_running_) {
        break;
      }
      std::cerr << "[Error] Failed to accept client.\n";
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(clients_mutex_);
      client_sockets_.push_back(client_socket);
    }

    client_threads.emplace_back(&TCPServer::handleClient, this, client_socket);
  }

  if (console_thread.joinable()) {
    console_thread.join();
  }

  for (auto &th : client_threads) {
    if (th.joinable()) {
      th.join();
    }
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
void TCPServer::broadcastMessage(const std::string &message,
                                 int sender_socket) {
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
  bool is_first_message = true;
  bool successfully_joined = false;

  while (true) {
    std::string received_msg;

    if (!receiveMessage(client_socket, received_msg)) {
      if (!is_running_) {
        break;
      }

      std::lock_guard<std::mutex> lock(clients_mutex_);
      client_sockets_.erase(std::remove(client_sockets_.begin(),
                                        client_sockets_.end(), client_socket),
                            client_sockets_.end());
      close(client_socket);

      if (successfully_joined) {
          std::cout << "[Info] Client " << client_socket << " disconnected." << std::endl;
          std::string disconnect_msg = "[Server]: Someone left the chat.";
          for (int fd : client_sockets_) {
            sendMessage(fd, disconnect_msg);
          }
          std::cout << "[Debug] Client thread for socket " << client_socket << " safely closed." << std::endl;
      }
      break;
    }

    if (!successfully_joined) {
        std::cout << "[Info] Verified real user connected! Socket: " << client_socket << std::endl;
        successfully_joined = true;
    }

    if (received_msg.find_first_not_of(" \t\n\r\f\v") == std::string::npos) {
      continue;
    }

    std::string username = "System";
    std::string content = received_msg;

    size_t colon_pos = received_msg.find("]: ");
    if (received_msg[0] == '[' && colon_pos != std::string::npos) {
      username = received_msg.substr(1, colon_pos - 1);
      content = received_msg.substr(colon_pos + 3);
    }

    if (username != "Server") {
      saveMessageToDB(username, content);
    }

    broadcastMessage(received_msg, client_socket);

    if (is_first_message) {
      is_first_message = false;
      sendHistoryToClient(client_socket);
    }
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
      shutdown(fd, SHUT_RDWR); // Примусово перериваємо всі операції читання/запису
      close(fd);                     // Закриваємо його сокет
    }
    client_sockets_.clear(); // Очищаємо список
  }

  // Перевіряємо, чи сокет сервера ще відкритий
  if (server_fd_ != -1) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_); // Тепер безпечно закриваємо
    server_fd_ = -1;
  }
}

/**
 * Зберігає повідомлення у базу даних PostgreSQL
 */
void TCPServer::saveMessageToDB(const std::string &username,
                                const std::string &content) {
  try {
    // Відкриваємо з'єднання
    pqxx::connection conn(DB_CONN);
    pqxx::work txn(conn); // Транзакція для запису

    // Використовуємо підготовлений запит для захисту від SQL-ін'єкцій
    txn.exec_params("INSERT INTO messages (username, content) VALUES ($1, $2)",
                    username, content);
    txn.commit(); // Підтверджуємо запис

  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Failed to save message: " << e.what() << "\n";
  }
}

/**
 * Дістає 25 останніх повідомлень та відправляє їх клієнту
 */
void TCPServer::sendHistoryToClient(int client_socket) {
  try {
    pqxx::connection conn(DB_CONN);
    pqxx::nontransaction txn(conn); // Транзакція тільки для читання

    // SQL-запит: беремо 25 останніх повідомлень і перевертаємо їх у
    // хронологічному порядку
    pqxx::result res = txn.exec(
        "SELECT username, content FROM ("
        "  SELECT id, username, content FROM messages ORDER BY id DESC LIMIT 25"
        ") AS sub ORDER BY id ASC;");

    if (!res.empty()) {
      sendMessage(client_socket, "\n--- Chat History (Last 25 messages) ---");
      for (auto row : res) {
        // Форматуємо назад у вигляд [Username]: Text
        std::string user = row["username"].c_str();
        std::string text = row["content"].c_str();
        std::string formatted_msg = "[" + user + "]: " + text;
        sendMessage(client_socket, formatted_msg);
      }
      sendMessage(client_socket, "---------------------------------------\n");
    }
  } catch (const std::exception &e) {
    std::cerr << "[DB Error] Failed to fetch history: " << e.what() << "\n";
  }
}