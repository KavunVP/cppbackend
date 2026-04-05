cmake_minimum_required(VERSION 3.11)
project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# обратите внимание на аргумент TARGETS у команды conan_basic_setup
include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Статическая библиотека с игровой моделью
add_library(game_model STATIC
    src/model.h
    src/model.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/tagged.h
)

target_include_directories(game_model PUBLIC src)

# Исполняемый файл сервера
add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.h
    src/request_handler_impl.h
    src/logging.cpp
    src/logging.h
    src/extra_data.h
)

target_link_libraries(game_server PRIVATE CONAN_PKG::boost Threads::Threads game_model)

# Тесты
add_executable(game_server_tests
    tests/model-tests.cpp
    tests/loot_generator_tests.cpp
)

target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 Threads::Threads game_model)

# Интеграция с CTest
include(CTest)

# Путь к Catch.cmake зависит от конфигурации Conan
set(CATCH2_CMAKE_PATH "")
if(CONAN_BUILD_DIRS_CATCH2)
    set(CATCH2_CMAKE_PATH "${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake")
elseif(CONAN_BUILD_DIRS_CATCH2_RELEASE)
    set(CATCH2_CMAKE_PATH "${CONAN_BUILD_DIRS_CATCH2_RELEASE}/Catch.cmake")
elseif(CONAN_BUILD_DIRS_CATCH2_DEBUG)
    set(CATCH2_CMAKE_PATH "${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake")
endif()

if(CATCH2_CMAKE_PATH)
    include(${CATCH2_CMAKE_PATH})
    catch_discover_tests(game_server_tests)
endif()
