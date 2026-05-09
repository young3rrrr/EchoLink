cmake_minimum_required(VERSION 3.10)
project(EchoLink VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

add_executable(echolink_server 
    src/main_server.cpp 
    src/network/TCPServer.cpp
)

add_executable(echolink_client 
    src/main_client.cpp 
    src/network/TCPClient.cpp
)

# Лінкуємо сервер з бібліотекою PostgreSQL
target_link_libraries(echolink_server PRIVATE pqxx pq)