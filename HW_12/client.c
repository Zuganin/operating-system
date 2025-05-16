
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 5000
#define BUF_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in addr;
    char buffer[BUF_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Клиент запущен. Ожидание сообщений...\n");
    while (1) {
        ssize_t n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0, NULL, NULL);
        if (n < 0) {
            perror("recvfrom");
            break;
        }
        buffer[n] = '\0';
        printf("Получено сообщение: %s\n", buffer);
    }
    close(sockfd);
    return 0;
}
