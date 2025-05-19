// filepath: /Users/vadimzenin/ДЗшки/operating-system/IHW_3/6-7/monitor.c
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define RCVBUFSIZE 1024

void DieWithError(char *errorMessage);
void CleanupAndExit(int signum);

int monitorSocket = -1;

int main(int argc, char *argv[])
{
    int sock;                        /* Дескриптор сокета */
    struct sockaddr_in servAddr;     /* Адрес сервера */
    unsigned short servPort;         /* Порт сервера */
    char *servIP;                    /* IP-адрес сервера (точечная нотация) */
    char buffer[RCVBUFSIZE];         /* Буфер для данных */
    int bytesRcvd;                   /* Количество байт, прочитанных за один recv() */
    
    if (argc != 3)  
    {
        fprintf(stderr, "Использование: %s <IP-адрес сервера> <Порт сервера>\n", argv[0]);
        exit(1);
    }
    
    servIP = argv[1];    
    servPort = atoi(argv[2]); 
    
    /* Создаем сокет для соединения с сервером */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("Ошибка при создании сокета");
    
    /* Сохраняем дескриптор сокета в глобальной переменной для доступа из обработчика сигнала */
    monitorSocket = sock;
    
    /* Устанавливаем обработчик сигнала для корректного завершения */
    signal(SIGINT, CleanupAndExit);
        
    /* Конструируем адрес сервера */
    memset(&servAddr, 0, sizeof(servAddr));     /* Обнуляем структуру */
    servAddr.sin_family      = AF_INET;         /* Семейство адресов Интернета */
    servAddr.sin_addr.s_addr = inet_addr(servIP);  /* IP-адрес сервера */
    servAddr.sin_port        = htons(servPort); /* Порт сервера */
    
    /* Устанавливаем соединение с сервером */
    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
        DieWithError("Ошибка при установке соединения");
        
    printf("\n[Монитор] =========================\n");
    printf("[Монитор] Подключение: Монитор подключился к серверу по адресу %s:%d\n", servIP, servPort);
    printf("[Монитор] =========================\n");
    
    /* Отправляем серверу специальное приветственное сообщение */
    strcpy(buffer, "MONITOR\n");
    if (send(sock, buffer, strlen(buffer), 0) != strlen(buffer))
        DieWithError("Ошибка при отправке приветственного сообщения");
    
    printf("[Монитор] Отправлено приветственное сообщение серверу\n");
    printf("[Монитор] Ожидание данных мониторинга...\n");

    /* Основной цикл получения и отображения данных мониторинга */
    while (1) {
        /* Получаем сообщение от сервера */
        if ((bytesRcvd = recv(sock, buffer, RCVBUFSIZE - 1, 0)) <= 0) {
            if (bytesRcvd == 0) {
                printf("[Монитор] Сервер закрыл соединение\n");
                break;
            } else {
                DieWithError("Ошибка при получении сообщения");
            }
        }
        
        /* Добавляем нулевой символ в конце полученной строки */
        buffer[bytesRcvd] = '\0';
        
        /* Выводим полученное сообщение без визуального разделения для компактности */
        printf("%s\n", buffer);
        
        /* Проверяем, содержит ли сообщение информацию о нахождении Винни-Пуха */
        if (strstr(buffer, "УСПЕХ: Стая пчел") && strstr(buffer, "Винни-Пух найден")) {
            printf("[Монитор] ЗАВЕРШЕНИЕ: Винни-Пух найден, завершаем работу монитора\n");
            break;
        }
    }
    
    printf("[Монитор] ОТКЛЮЧЕНИЕ: Отключаюсь от сервера.\n");
    printf("[Монитор] ЗАВЕРШЕНИЕ: Монитор завершил работу.\n");
    
    close(sock);
    exit(0);
}

/* Функция для корректного завершения программы по сигналу Ctrl+C */
void CleanupAndExit(int signum) {
    printf("\n[Монитор] СИГНАЛ: Получен сигнал завершения (%d). Завершаю работу монитора...\n", signum);
    
    /* Освобождение ресурсов */
    if (monitorSocket >= 0) {
        close(monitorSocket);
    }
    
    printf("[Монитор] ЗАВЕРШЕНИЕ: Монитор успешно завершил работу.\n");
    exit(0);
}
