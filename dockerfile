FROM gcc:latest

WORKDIR /app

COPY . .

RUN g++ -std=c++17 main.cpp -o rental

EXPOSE 10000

CMD ["./rental"]