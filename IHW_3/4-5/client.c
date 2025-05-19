#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define RCVBUFSIZE 1024

void DieWithError(char *errorMessage);
void CleanupAndExit(int signum);

int winnieFound = 0; // Инициализируем явно, чтобы не было мусора
int clientSocket = -1;

int main(int argc, char *argv[])
{
    int sock;                        /* Дескриптор сокета */
    struct sockaddr_in servAddr;     /* Адрес сервера */
    unsigned short servPort;         /* Порт сервера */
    char *servIP;                    /* IP-адрес сервера (точечная нотация) */
    char buffer[RCVBUFSIZE];         /* Буфер для строки */
    char responseBuffer[RCVBUFSIZE]; /* Буфер для ответа */
    unsigned int bufferLen;          /* Длина строки */
    int bytesRcvd;                   /* Количество байт, прочитанных за один recv() */
    int swarmID;                     /* ID стаи пчел */
    int regionToCheck;               /* Регион для проверки */
    int containsWinnie;              /* Содержит ли регион Винни-Пуха */
    int searchComplete = 0;          /* Флаг завершения поиска (0 - не завершен, 1 - завершен) */
    
    if (argc != 3)  
    {
        fprintf(stderr, "Использование: %s <IP-адрес сервера> <Порт сервера>\n", argv[0]);
        exit(1);
    }
    
    srand(time(NULL));
    
    swarmID = rand() % 1000 + 1;
    
    servIP = argv[1];    
    servPort = atoi(argv[2]); 
    
    /* Создаем сокет для соединения с сервером */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("Ошибка при создании сокета");
    
    /* Сохраняем дескриптор сокета в глобальной переменной для доступа из обработчика сигнала */
    clientSocket = sock;
    
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
        
    printf("\n[Клиент #%d] =========================\n", swarmID);
    printf("[Клиент #%d] Подключение: Стая пчел подключилась к улью по адресу %s:%d\n", swarmID, servIP, servPort);
    printf("[Клиент #%d] =========================\n", swarmID);
    
    /* Отправляем серверу инициализационное сообщение */
    sprintf(buffer, "INIT %d", swarmID);
    if (send(sock, buffer, strlen(buffer), 0) != strlen(buffer))
        DieWithError("Ошибка при отправке сообщения");

    // Ждем подтверждения инициализации (сервер сразу начинает цикл CHECK)
    // Основной цикл работы клиента
    while (searchComplete == 0) {
        /* Получаем сообщение от сервера */
        if ((bytesRcvd = recv(sock, responseBuffer, RCVBUFSIZE, 0)) <= 0) {
            if (bytesRcvd == 0) {
                // Если соединение закрыто сервером до получения финального сообщения,
                // явно сообщаем, что медведя нашла другая стая
                printf("[Клиент #%d] УСПЕХ: Винни-Пух был найден другой стаей.\n", swarmID);
                printf("[Клиент #%d] ВОЗВРАТ: Завершаю поиск и возвращаюсь в улей.\n", swarmID);
                searchComplete = 1;
                break;
            } else {
                DieWithError("Ошибка при получении сообщения или соединение преждевременно закрыто");
            }
        }
        responseBuffer[bytesRcvd] = '\0'; // Завершаем строку нулевым символом
        if (strncmp(responseBuffer, "CHECK", 5) == 0) {
            if (sscanf(responseBuffer, "CHECK %d %d", &regionToCheck, &containsWinnie) != 2) {
                DieWithError("Неверный формат сообщения CHECK");
            }
            printf("[Клиент #%d] ПОИСК: Проверяю регион %d на наличие Винни-Пуха...\n", swarmID, regionToCheck);
            sleep(1 + rand() % 2);
            if (containsWinnie == 1) {
                printf("[Клиент #%d] НАЙДЕН: Винни-Пух найден в регионе %d! Наказываю Винни-Пуха...\n", swarmID, regionToCheck);
                sprintf(buffer, "FOUND %d", regionToCheck);
                if (send(sock, buffer, strlen(buffer), 0) != strlen(buffer))
                    DieWithError("Ошибка при отправке сообщения");
            } else {
                printf("[Клиент #%d] ОСМОТР: Регион %d чист, Винни-Пуха здесь нет.\n", swarmID, regionToCheck);
                sprintf(buffer, "NOTFOUND %d", regionToCheck);
                if (send(sock, buffer, strlen(buffer), 0) != strlen(buffer))
                    DieWithError("Ошибка при отправке сообщения");
            }
        } else if (strncmp(responseBuffer, "CONFIRM", 7) == 0) {
            printf("[Клиент #%d] УСПЕХ: %s\n", swarmID, responseBuffer + 8);
            printf("[Клиент #%d] ВОЗВРАТ: Задание выполнено! Возвращаюсь в улей.\n", swarmID);
            winnieFound = 1;
            searchComplete = 1;
        } else if (strncmp(responseBuffer, "DONE", 4) == 0) {
            printf("[Клиент #%d] ИНФОРМАЦИЯ: %s\n", swarmID, responseBuffer + 5);
            printf("[Клиент #%d] ВОЗВРАТ: Поиск завершен. Возвращаюсь в улей.\n", swarmID);
            winnieFound = 1;
            searchComplete = 1;
        } else if (strncmp(responseBuffer, "FINISH", 6) == 0) {
            printf("[Клиент #%d] ИНФОРМАЦИЯ: %s\n", swarmID, responseBuffer + 7);
            winnieFound = 1;
            searchComplete = 1;
        } else {
            printf("[Клиент #%d] ПРЕДУПРЕЖДЕНИЕ: Получено неизвестное сообщение: %s\n", swarmID, responseBuffer);
        }
    }
    printf("[Клиент #%d] ОТКЛЮЧЕНИЕ: Отключаюсь от сервера.\n", swarmID);
    printf("[Клиент #%d] ЗАВЕРШЕНИЕ: Клиент завершил работу.\n", swarmID);
    close(sock);
    exit(0);
}

/* Функция для корректного завершения программы по сигналу Ctrl+C */
void CleanupAndExit(int signum) {
    printf("\n[Клиент] СИГНАЛ: Получен сигнал завершения (%d). Завершаю работу клиента...\n", signum);
    
    /* Освобождение ресурсов */
    if (clientSocket >= 0) {
        close(clientSocket);
    }
    
    printf("[Клиент] ЗАВЕРШЕНИЕ: Клиент успешно завершил работу.\n");
    exit(0);
}

