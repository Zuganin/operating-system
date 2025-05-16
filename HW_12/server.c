
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BROADCAST_IP "255.255.255.255"
#define PORT 5000
#define BUF_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in broadcast_addr;
    int broadcast_enable = 1;
    char buffer[BUF_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("setsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    printf("Сервер запущен. Введите сообщения для рассылки (Ctrl+C для выхода):\n");
    while (fgets(buffer, BUF_SIZE, stdin) != NULL) {
        size_t len = strlen(buffer);
        if (buffer[len-1] == '\n') buffer[len-1] = '\0';
        if (sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
            perror("sendto failed");
        }
    }
    close(sockfd);
    return 0;
}
