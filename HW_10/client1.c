// tcp_client1.c
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

    printf("[Клиент 1] Введите сообщения (The End для завершения):\n");
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t len = strlen(buffer);
        if (send(sock, buffer, len, 0) != (ssize_t)len)
            DieWithError("send() failed");
        if (strcmp(buffer, "The End\n") == 0)
            break;
    }
    close(sock);
    printf("[Клиент 1] Соединение закрыто.\n");
    return 0;
}