// Файл: include/NetworkUtils.h
#pragma once

#include <string>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>

// Используем inline, чтобы избежать ошибок двойного определения при подключении в разные файлы
inline bool sendMessage(int socket, const std::string& message) {
    uint32_t msgSize = message.size();
    uint32_t netSize = htonl(msgSize); 

    // Сначала отправляем размер (4 байта)
    if (send(socket, &netSize, sizeof(netSize), 0) == -1) {
        return false; 
    }

    // Затем отправляем сами данные
    size_t totalSent = 0;
    while (totalSent < msgSize) {
        ssize_t sent = send(socket, message.c_str() + totalSent, msgSize - totalSent, 0);
        if (sent == -1) {
            return false; 
        }
        totalSent += sent;
    }
    return true; 
}

inline bool receiveMessage(int socket, std::string& outMessage) {
    uint32_t netSize = 0;
    
    // Сначала читаем размер (ждем ровно 4 байта)
    ssize_t headerBytesRead = recv(socket, &netSize, sizeof(netSize), MSG_WAITALL);
    if (headerBytesRead <= 0) {
        return false; 
    }

    uint32_t msgSize = ntohl(netSize); 
    if (msgSize == 0) {
        outMessage = "";
        return true;
    }

    // Выделяем динамический буфер точно под размер сообщения
    std::vector<char> buffer(msgSize);
    size_t totalRead = 0;

    // Читаем само сообщение
    while (totalRead < msgSize) {
        ssize_t bytesRead = recv(socket, buffer.data() + totalRead, msgSize - totalRead, 0);
        if (bytesRead <= 0) {
            return false; 
        }
        totalRead += bytesRead;
    }

    outMessage.assign(buffer.begin(), buffer.end());
    return true;
}