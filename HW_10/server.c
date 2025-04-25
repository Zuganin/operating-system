// tcp_server.c - однопоточная версия
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>  // Для fcntl()

#define BUFFER_SIZE 1024

int client1_fd = -1;
int client2_fd = -1;
int server_fd = -1;

void DieWithError(char *errorMessage) {
    perror(errorMessage);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <порт>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        DieWithError("socket");
    }
    
    // Опция для повторного использования адреса
    int optval = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        DieWithError("setsockopt");
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        DieWithError("bind");
    }
    
    if (listen(server_fd, 2) < 0) {
        DieWithError("listen");
    }

    printf("[Сервер] Ожидание подключения Client1 на порту %d...\n", port);
    client1_fd = accept(server_fd, NULL, NULL);
    if (client1_fd < 0) {
        DieWithError("accept (client1)");
    }
    printf("[Сервер] Client1 подключен.\n");

    printf("[Сервер] Ожидание подключения Client2...\n");
    client2_fd = accept(server_fd, NULL, NULL);
    if (client2_fd < 0) {
        DieWithError("accept (client2)");
    }
    printf("[Сервер] Client2 подключен.\n");

    // Основной цикл обработки сообщений
    char buffer[BUFFER_SIZE];
    while (1) {
        ssize_t bytes_read = recv(client1_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read < 0) {
            DieWithError("recv");
        }
        if (bytes_read == 0) {
            printf("[Сервер] Client1 закрыл соединение\n");
            break;
        }
        buffer[bytes_read] = '\0';
        printf("[Сервер] Получено от Client1: %s", buffer);

        if (send(client2_fd, buffer, bytes_read, 0) != bytes_read) {
            DieWithError("send to client2");
        }
        printf("[Сервер] Переслано Client2: %s", buffer);

        if (strcmp(buffer, "The End\n") == 0) {
            printf("[Сервер] Получена команда завершения работы\n");
            break;
        }
    }

    // Закрытие соединений
    close(client1_fd);
    close(client2_fd);
    close(server_fd);
    printf("[Сервер] Соединения закрыты.\n");
    
    return 0;
}
