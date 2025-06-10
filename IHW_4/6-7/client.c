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
#define DEFAULT_SERVER_IP "127.0.0.1"
#define REQUEST_TASK "GET_TASK"
#define SUBMIT_RESULT "SUBMIT_RESULT"
#define NO_MORE_TASKS "NO_TASKS"
#define TASK_RECEIVED "TASK_RECEIVED"

// Константы для протокола монитора
#define CLIENT_EVENT "CLIENT_EVENT"

// Глобальные переменные для обработки сигналов
volatile sig_atomic_t client_running = 1;
int sockfd_global = -1;

// Обработчик SIGINT
void sigint_handler(int sig) {
    (void)sig; // Убираем warning о неиспользуемом параметре
    printf("\nПолучен сигнал SIGINT. Завершение работы клиента...\n");
    client_running = 0;
    if (sockfd_global != -1) {
        close(sockfd_global);
    }
    exit(0);
}

// Функция шифрования "пляшущими человечками" (ASCII коды)
char* encrypt_dancing_men(const char *text) {
    int len = strlen(text);
    // Каждый символ превратится в число до 3 цифр + пробел
    char *result = malloc(len * 4 + 1);
    if (!result) {
        perror("Ошибка выделения памяти для шифрования");
        return NULL;
    }
    
    result[0] = '\0';
    char temp[10];
    
    for (int i = 0; i < len; i++) {
        sprintf(temp, "%d ", (int)text[i]);
        strcat(result, temp);
    }
    
    return result;
}

// Функция для отправки сообщения монитору
void send_to_monitor(const char *message, const char *server_ip) {
    int monitor_sockfd;
    struct sockaddr_in monitor_addr;
    char full_message[BUFFER_SIZE];
    
    monitor_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (monitor_sockfd < 0) {
        return; // Если не удалось создать сокет, просто игнорируем
    }
    
    memset(&monitor_addr, 0, sizeof(monitor_addr));
    monitor_addr.sin_family = AF_INET;
    monitor_addr.sin_port = htons(DEFAULT_PORT);
    inet_pton(AF_INET, server_ip, &monitor_addr.sin_addr);
    
    snprintf(full_message, BUFFER_SIZE, "%s:%s", CLIENT_EVENT, message);
    
    sendto(monitor_sockfd, full_message, strlen(full_message), 0,
           (struct sockaddr*)&monitor_addr, sizeof(monitor_addr));
    
    close(monitor_sockfd);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    char *server_ip = DEFAULT_SERVER_IP;
    int port = DEFAULT_PORT;
    int task_count = 0;
    
    // Инициализируем генератор случайных чисел
    srand(time(NULL));
    
    // Парсинг аргументов командной строки
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
    
    if (argc == 1) {
        printf("Использование: %s [IP_сервера] [порт]\n", argv[0]);
        printf("По умолчанию: %s %d\n", DEFAULT_SERVER_IP, DEFAULT_PORT);
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
        perror("Неверный IP адрес");
        close(sockfd);
        exit(1);
    }
    
    printf("=== КЛИЕНТ-ШИФРОВАЛЬЩИК ПЛЯШУЩИХ ЧЕЛОВЕЧКОВ ===\n");
    printf("Подключение к серверу %s:%d\n", server_ip, port);
    
    // Попытка подключения к серверу с повторными попытками
    int connection_attempts = 0;
    int max_attempts = 15;
    
    while (client_running && connection_attempts < max_attempts) {
        // Отправляем запрос задачи серверу
        if (sendto(sockfd, REQUEST_TASK, strlen(REQUEST_TASK), 0,
                   (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Ошибка отправки запроса");
            connection_attempts++;
            printf("Попытка подключения %d/%d...\n", connection_attempts, max_attempts);
            sleep(1);
            continue;
        }
        
        // Получение ответа от сервера с таймаутом
        socklen_t server_len = sizeof(server_addr);
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = 2;  // 2 секунды ожидания
        timeout.tv_usec = 0;
        
        int select_result = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        
        if (select_result < 0) {
            if (client_running) {
                perror("Ошибка select");
            }
            break;
        } else if (select_result == 0) {
            // Таймаут - сервер не отвечает
            connection_attempts++;
            if (connection_attempts < max_attempts) {
                sleep(1);
                continue;
            } else {
                printf("Не удалось подключиться к серверу после %d попыток.\n", max_attempts);
                break;
            }
        }
        
        int received_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                                     (struct sockaddr*)&server_addr, &server_len);
        
        if (received_bytes < 0) {
            if (client_running) {
                perror("Ошибка получения данных");
            }
            break;
        }
        
        // Сброс счетчика попыток при успешном получении данных
        connection_attempts = 0;
        
        buffer[received_bytes] = '\0';
        
        // Проверяем, не закончились ли задачи
        if (strcmp(buffer, NO_MORE_TASKS) == 0) {
            printf("Все задачи выполнены. Клиент завершает работу.\n");
            send_to_monitor("Все задачи выполнены - клиент завершает работу", server_ip);
            break;
        }
        
        // Парсим ответ: позиция:задача
        char *colon_pos = strchr(buffer, ':');
        if (!colon_pos) {
            printf("Некорректный формат ответа от сервера\n");
            continue;
        }
        
        *colon_pos = '\0';
        int position = atoi(buffer);
        char *task_text = colon_pos + 1;
        
        task_count++;
        printf("\nПолучена задача %d (позиция %d): %s\n", task_count, position, task_text);
        
        // Уведомляем монитор о получении задачи
        char message[BUFFER_SIZE];
        snprintf(message, BUFFER_SIZE, "Получена задача %d (позиция %d)", task_count, position);
        send_to_monitor(message, server_ip);
        
        // Шифруем текст
        printf("Шифрование фрагмента...\n");
        char *encrypted = encrypt_dancing_men(task_text);
        if (!encrypted) {
            printf("Ошибка шифрования\n");
            continue;
        }
        
        printf("Зашифрованный фрагмент: %s\n", encrypted);
        
        // Имитируем время обработки (асинхронность)
        usleep((rand() % 500 + 100) * 1000); // 100-600 мс
        
        // Отправляем результат серверу: SUBMIT_RESULT:позиция:зашифрованный_текст
        snprintf(response, BUFFER_SIZE, "%s:%d:%s", SUBMIT_RESULT, position, encrypted);
        
        if (sendto(sockfd, response, strlen(response), 0,
                   (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Ошибка отправки результата");
            free(encrypted);
            break;
        }
        
        printf("Результат отправлен серверу\n");
        send_to_monitor("Результат отправлен серверу", server_ip);
        free(encrypted);
        
        // Ждем подтверждения
        received_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                                 (struct sockaddr*)&server_addr, &server_len);
        if (received_bytes > 0) {
            buffer[received_bytes] = '\0';
            if (strcmp(buffer, TASK_RECEIVED) == 0) {
                printf("Подтверждение получено от сервера\n");
                send_to_monitor("Подтверждение получено от сервера", server_ip);
            }
        }
    }
    
    if (connection_attempts >= max_attempts) {
        printf("Превышено максимальное количество попыток подключения.\n");
    }
    
    close(sockfd);
    printf("Клиент обработал %d фрагментов.\n", task_count);
    return 0;
}