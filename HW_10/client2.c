// tcp_client2.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define BUFFER_SIZE 1024

void DieWithError(char *errorMessage) {
    perror(errorMessage);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Server IP> <Server Port>\n", argv[0]);
        exit(1);
    }
    int sock;
    struct sockaddr_in servAddr;
    unsigned short servPort = atoi(argv[2]);
    char *servIP = argv[1];
    char buffer[1024];

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(servIP);
    servAddr.sin_port = htons(servPort);

    if (connect(sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
        DieWithError("connect() failed");

    printf("[Клиент 2] Ожидание сообщений...\n");
    while (1) {
        ssize_t recvSize = recv(sock, buffer, sizeof(buffer)-1, 0);
        if (recvSize < 0)
            DieWithError("recv() failed");
        if (recvSize == 0) {
            printf("[Клиент 2] Сервер закрыл соединение\n");
            break;
        }
        buffer[recvSize] = '\0';
        printf("[Клиент 2] Получено: %s", buffer);
        if (strcmp(buffer, "The End\n") == 0)
            break;
    }
    close(sock);
    printf("[Клиент 2] Соединение закрыто.\n");
    return 0;
}
