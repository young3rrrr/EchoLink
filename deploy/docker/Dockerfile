FROM debian:latest

RUN apt update && apt install git cmake build-essential libpqxx-dev -y

WORKDIR /app

COPY . /app

RUN mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SERVER=ON -DBUILD_CLIENT=OFF && make

CMD ["./build/echolink_server"]