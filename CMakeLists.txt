cmake_minimum_required(VERSION 3.14) # Підняли версію для FetchContent
project(EchoLink VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

include_directories(include ${PQXX_INCLUDE_DIRS})
link_directories(${PQXX_LIBRARY_DIRS})

# --- Завантаження FTXUI ---
include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG main
)
FetchContent_MakeAvailable(ftxui)
# --------------------------

add_executable(echolink_server 
    src/main_server.cpp 
    src/network/TCPServer.cpp
)

add_executable(echolink_client 
    src/main_client.cpp 
    src/network/TCPClient.cpp
)

# Лінкуємо потоки ТА компоненти FTXUI до клієнта
target_link_libraries(echolink_client PRIVATE 
    Threads::Threads 
    ftxui::screen 
    ftxui::dom 
    ftxui::component
)

# Сервер залишається без змін (з БД)
target_link_libraries(echolink_server PRIVATE ${PQXX_LIBRARIES} Threads::Threads)