cmake_minimum_required(VERSION 3.11)
project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# Используем тулчейн от Conan (вместо conan_basic_setup)
include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake)

# Находим нужные компоненты явно
find_package(Boost 1.78.0 REQUIRED COMPONENTS log log_setup program_options json)
find_package(Threads REQUIRED)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.cpp
    src/request_handler.h
    src/logging.cpp          
    src/logging.h            
)

# Линкуем конкретные таргеты
target_link_libraries(game_server PRIVATE
    Threads::Threads
    Boost::log
    Boost::log_setup
    Boost::json
    Boost::program_options
)