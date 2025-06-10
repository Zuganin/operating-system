#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <sys/select.h>

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 1024
#define LARGE_BUFFER_SIZE 8192  // Для длинных сообщений
#define DEFAULT_SERVER_IP "127.0.0.1"

// Константы для протокола монитора
#define MONITOR_REGISTER "REGISTER_MONITOR"
#define MONITOR_ACK "MONITOR_ACK"
#define SERVER_EVENT "SERVER_EVENT"
#define CLIENT_EVENT "CLIENT_EVENT"
#define SYSTEM_EVENT "SYSTEM_EVENT"
#define MONITOR_SHUTDOWN "MONITOR_SHUTDOWN"

// Глобальные переменные для обработки сигналов
volatile sig_atomic_t monitor_running = 1;
int sockfd_global = -1;

// Обработчик SIGINT
void sigint_handler(int sig) {
    (void)sig;
    printf("\n[МОНИТОР] Получен сигнал SIGINT. Завершение работы монитора...\n");
    monitor_running = 0;
    if (sockfd_global != -1) {
        close(sockfd_global);
    }
    exit(0);
}

// Функция для получения текущего времени в строковом формате
void get_timestamp(char *timestamp_str, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp_str, size, "%H:%M:%S", t);
}

// Функция для вывода статистики
void print_statistics() {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    printf("\n=== СТАТИСТИКА СИСТЕМЫ [%s] ===\n", timestamp);
    printf("Монитор активен и отслеживает события системы\n");
    printf("==========================================\n\n");
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr;
    char *buffer = malloc(LARGE_BUFFER_SIZE);  // Используем динамический буфер
    char *server_ip = DEFAULT_SERVER_IP;
    int port = DEFAULT_PORT;
    char timestamp[32];
    
    if (!buffer) {
        printf("Ошибка выделения памяти для буфера\n");
        exit(1);
    }
    
    // Обработка аргументов командной строки
    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            printf("Некорректный порт. Используется порт по умолчанию: %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
    }
    
    // Установка обработчика сигналов
    signal(SIGINT, sigint_handler);
    
    // Создание UDP сокета
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка создания сокета");
        exit(1);
    }
    
    sockfd_global = sockfd;
    
    // Настройка адреса сервера
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Некорректный IP адрес сервера: %s\n", server_ip);
        close(sockfd);
        exit(1);
    }
    
    printf("=== МОНИТОР СИСТЕМЫ ШИФРОВАНИЯ ===\n");
    printf("Подключение к серверу %s:%d\n", server_ip, port);
    printf("Система поддерживает множественные мониторы\n");
    
    // Регистрация монитора на сервере
    if (sendto(sockfd, MONITOR_REGISTER, strlen(MONITOR_REGISTER), 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Ошибка регистрации монитора");
        close(sockfd);
        exit(1);
    }
    
    get_timestamp(timestamp, sizeof(timestamp));
    printf("[%s] Запрос на регистрацию монитора отправлен\n", timestamp);
    
    // Ожидание подтверждения регистрации
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    fd_set read_fds;
    struct timeval timeout;
    
    // Устанавливаем таймаут для ожидания ответа сервера
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    int select_result = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
    
    if (select_result <= 0) {
        printf("Сервер не отвечает или недоступен. Монитор будет работать в автономном режиме.\n");
        print_statistics();
    } else {
        int received = recvfrom(sockfd, buffer, LARGE_BUFFER_SIZE - 1, 0,
                               (struct sockaddr*)&from_addr, &from_len);
        if (received > 0) {
            buffer[received] = '\0';
            if (strcmp(buffer, MONITOR_ACK) == 0) {
                get_timestamp(timestamp, sizeof(timestamp));
                printf("[%s] Монитор успешно зарегистрирован на сервере\n", timestamp);
                printf("[%s] Начинаем отслеживание событий системы...\n", timestamp);
                print_statistics();
            }
        }
    }
    
    // Основной цикл монитора
    while (monitor_running) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        
        // Устанавливаем таймаут для периодического вывода статистики
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        
        select_result = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (select_result == 0) {
            // Таймаут - выводим статистику
            continue;
        } else if (select_result > 0 && FD_ISSET(sockfd, &read_fds)) {
            // Получено сообщение от сервера
            from_len = sizeof(from_addr);
            int received = recvfrom(sockfd, buffer, LARGE_BUFFER_SIZE - 1, 0,
                                   (struct sockaddr*)&from_addr, &from_len);
            
            if (received > 0) {
                buffer[received] = '\0';
                get_timestamp(timestamp, sizeof(timestamp));
                
                // Парсинг и вывод различных типов событий
                if (strncmp(buffer, MONITOR_SHUTDOWN, strlen(MONITOR_SHUTDOWN)) == 0) {
                    char *event_data = buffer + strlen(MONITOR_SHUTDOWN) + 1;
                    printf("[%s] ЗАВЕРШЕНИЕ: %s\n", timestamp, event_data);
                    printf("[%s] Сервер завершил работу. Монитор автоматически завершается.\n", timestamp);
                    monitor_running = 0;
                    break;
                }
                else if (strncmp(buffer, SERVER_EVENT, strlen(SERVER_EVENT)) == 0) {
                    char *event_data = buffer + strlen(SERVER_EVENT) + 1;
                    printf("[%s] СЕРВЕР: %s\n", timestamp, event_data);
                } 
                else if (strncmp(buffer, CLIENT_EVENT, strlen(CLIENT_EVENT)) == 0) {
                    char *event_data = buffer + strlen(CLIENT_EVENT) + 1;
                    printf("[%s] КЛИЕНТ: %s\n", timestamp, event_data);
                }
                else if (strncmp(buffer, SYSTEM_EVENT, strlen(SYSTEM_EVENT)) == 0) {
                    char *event_data = buffer + strlen(SYSTEM_EVENT) + 1;
                    
                    // Проверяем специальные системные события для красивого отображения
                    if (strstr(event_data, "ИСХОДНЫЙ ТЕКСТ:") != NULL) {
                        printf("[%s] %s\n", timestamp, event_data);
                    }
                    else if (strstr(event_data, "ИТОГОВЫЙ ЗАШИФРОВАННЫЙ ТЕКСТ:") != NULL) {
                        printf("[%s] %s\n", timestamp, event_data);
                    }
                    else {
                        printf("[%s] СИСТЕМА: %s\n", timestamp, event_data);
                    }
                }
                else {
                    printf("[%s] СОБЫТИЕ: %s\n", timestamp, buffer);
                }
            }
        }
    }
    
    close(sockfd);
    get_timestamp(timestamp, sizeof(timestamp));
    printf("[%s] Монитор завершил работу\n", timestamp);
    
    // Освобождаем память
    free(buffer);
    
    return 0;
}
