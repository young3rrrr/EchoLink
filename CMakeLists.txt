cmake_minimum_required(VERSION 3.10)
project(EchoLink VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

include_directories(include ${PQXX_INCLUDE_DIRS})
link_directories(${PQXX_LIBRARY_DIRS})

add_executable(echolink_server 
    src/main_server.cpp 
    src/network/TCPServer.cpp
)

add_executable(echolink_client 
    src/main_client.cpp 
    src/network/TCPClient.cpp
)

target_link_libraries(echolink_client PRIVATE Threads::Threads)

target_link_libraries(echolink_server PRIVATE ${PQXX_LIBRARIES} Threads::Threads)