# Не просто создаём образ, но даём ему имя build
FROM gcc:11.3 AS build

# Invalidate cache to workaround Docker BuildKit snapshot bug
ARG CACHEBUST=1

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# Запуск conan как раньше
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11

# Папка data больше не нужна
COPY ./src /app/src
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 AS run

# Скопируем приложение со сборочного контейнера в директорию /app.
COPY --from=build /app/build/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

# Запускаем игровой сервер
# ENTRYPOINT ["/app/game_server", "/app/data/config.json", "/app/wwwroot"]
