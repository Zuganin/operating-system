#include <stdio.h>
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <stdlib.h>
#include <string.h>     
#include <unistd.h>  
#include <time.h>
#include <pthread.h>   
#include <signal.h>  
#include <stdatomic.h> 
#include <errno.h>

#define MAXPENDING 10    
#define BUFSIZE 1024      

void DieWithError(char *errorMessage);  
void HandleTCPClient(int clntSocket, int *forest, int forestSize, int winnieRegion, int *checkedRegions); 
void CleanupAndExit(int signum);  

/* Функции для работы с мониторами */
int IsMonitor(char *message);
void AddMonitor(int socket);
void RemoveMonitor(int socket);
void SendToMonitors(const char *message);

int servSock;   
int winnieFound = 0; // глобальный флаг

/* Внешние переменные для работы с мониторами */
extern int monitorSockets[];
extern int monitorCount;

struct ThreadArgs
{
    int clntSock;                    /* Дескриптор сокета клиента */
    int *forest;                     /* Указатель на массив регионов леса */
    int forestSize;                  /* Размер леса */
    // int *winnieFound;             /* Удалено: теперь глобальная переменная */
    int winnieRegion;                /* Регион с Винни-Пухом */
    int *checkedRegions;             /* Указатель на проверяемый регион */
};

/* Основная функция потока */
void *ThreadMain(void *threadArgs);

int main(int argc, char *argv[])
{
    int clntSock;
    struct sockaddr_in ServAddr;
    struct sockaddr_in ClntAddr;
    unsigned short ServPort;
    unsigned int clntLen;
    int forestSize;
    int winnieRegion;
    int *forest;
    // int winnieFound = 0;          /* Удалено: теперь глобальная переменная */
    pthread_t threadID;
    struct ThreadArgs *threadArgs;
    
    /* Проверка аргументов командной строки */
    if (argc != 5)
    {
        fprintf(stderr, "Использование: %s <IP адрес сервера> <Порт сервера>  <Размер леса> <Регион Винни-Пуха>\n", argv[0]);
        exit(1);
    }

    ServPort = atoi(argv[2]);
    forestSize = atoi(argv[3]);
    winnieRegion = atoi(argv[4]);
    
    if (forestSize <= 0) {
        fprintf(stderr, "Размер леса должен быть положительным числом\n");
        exit(1);
    }
    
    if (winnieRegion < 0 || winnieRegion >= forestSize) {
        fprintf(stderr, "Регион Винни-Пуха должен быть от 0 до %d\n", forestSize - 1);
        exit(1);
    }
    
    srand(time(NULL));
    
    forest = (int *)calloc(forestSize, sizeof(int));
    if (forest == NULL) {
        DieWithError("Ошибка выделения памяти");
    }
    
    forest[winnieRegion] = 1; /* 1 - есть Винни-Пух, 0 - нет */
    int *checkedRegions = calloc(forestSize, sizeof(int)); // 0 - не проверен, 1 - проверен
    if (checkedRegions == NULL) {
        free(forest);
        DieWithError("Ошибка выделения памяти для проверенных регионов");
    }
    printf("\n[Сервер] ИНИЦИАЛИЗАЦИЯ: Лес имеет %d регионов. Винни-Пух находится в регионе %d\n", forestSize, winnieRegion);
    
    signal(SIGINT, CleanupAndExit);
    
    /* Создание сокета */
    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("Ошибка создания сокета");
    
    /* Установка опции SO_REUSEADDR для повторного использования порта */
    int optval = 1;
    if (setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
        DieWithError("Ошибка установки опции setsockopt()");

    memset(&ServAddr, 0, sizeof(ServAddr));
    ServAddr.sin_family = AF_INET;
    ServAddr.sin_addr.s_addr = inet_addr(argv[1]);
    ServAddr.sin_port = htons(ServPort);

    if (bind(servSock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
        DieWithError("Ошибка привязки сокета");

    if (listen(servSock, MAXPENDING) < 0)
        DieWithError("Ошибка прослушивания");

    printf("\n[Сервер] ЗАПУСК: Сервер запущен на %s:%d. Ожидание стай пчел...\n", 
           inet_ntoa(ServAddr.sin_addr), ntohs(ServAddr.sin_port));
    printf("------------------------------------------------------\n");
    
    /* Основной цикл сервера */
    while(winnieFound == 0) {

        clntLen = sizeof(ClntAddr);
        if ((clntSock = accept(servSock, (struct sockaddr *) &ClntAddr, &clntLen)) < 0) {
            if (winnieFound == 1) {
                // Сервер завершает работу, не считаем это ошибкой
                break;
            }
            // Если ошибка вызвана закрытием сокета (например, Software caused connection abort), просто выходим из цикла
            if (errno == EBADF || errno == ECONNABORTED) {
                break;
            }
            DieWithError("Ошибка принятия соединения");
        }
        
        /* Клиент подключен! Но пока не знаем, кто это - пчела или монитор */
        printf("[Сервер] ПОДКЛЮЧЕНИЕ: Новое подключение с %s:%d\n", 
               inet_ntoa(ClntAddr.sin_addr), ntohs(ClntAddr.sin_port));
        
        /* Уведомление о подключении пока не отправляем, 
           так как еще не знаем, пчела это или монитор.
           Это будет обработано в HandleTCPClient */
        
        /* Создаем отдельную память для аргументов клиента */
        if ((threadArgs = (struct ThreadArgs *) malloc(sizeof(struct ThreadArgs))) == NULL)
            DieWithError("malloc() не удалось");
        
        threadArgs->clntSock = clntSock;
        threadArgs->forest = forest;
        threadArgs->forestSize = forestSize;
        threadArgs->winnieRegion = winnieRegion;
        threadArgs->checkedRegions = checkedRegions;

        /* Создаем поток клиента */
        if (pthread_create(&threadID, NULL, ThreadMain, (void *) threadArgs) != 0)
            DieWithError("pthread_create() не удалось");

    };
    
    /* Освобождение ресурсов */
    free(forest);
    free(checkedRegions);
    printf("[Сервер] ЗАВЕРШЕНИЕ: Сервер завершает работу.\n");
    exit(0);
}

/* Функция потока для обработки клиентских подключений */
void *ThreadMain(void *threadArgs)
{
    int clntSock;
    int *forest;
    int forestSize;
    int winnieRegion;
    int *checkedRegions;
    
    pthread_detach(pthread_self());
    
    clntSock = ((struct ThreadArgs *) threadArgs)->clntSock;
    forest = ((struct ThreadArgs *) threadArgs)->forest;
    forestSize = ((struct ThreadArgs *) threadArgs)->forestSize;
    winnieRegion = ((struct ThreadArgs *) threadArgs)->winnieRegion;
    checkedRegions = ((struct ThreadArgs *) threadArgs)->checkedRegions;
    
    free(threadArgs); 
    
    HandleTCPClient(clntSock, forest, forestSize, winnieRegion, checkedRegions);
    
    return NULL;
}

/* Функция для корректного завершения программы по сигналу Ctrl+C */
void CleanupAndExit(int signum) {
    printf("\n[Сервер] СИГНАЛ: Получен сигнал завершения (%d). Завершаю работу сервера...\n", signum);
    
    /* Отправляем сообщение всем мониторам о завершении работы */
    SendToMonitors("Сервер завершает работу по сигналу");
    
    /* Закрываем сокеты мониторов */
    for (int i = 0; i < monitorCount; i++) {
        if (monitorSockets[i] >= 0) {
            close(monitorSockets[i]);
        }
    }
    
    if (servSock >= 0) {
        close(servSock);
    }
    
    printf("[Сервер] ЗАВЕРШЕНИЕ: Сервер успешно завершил работу.\n");
    exit(0);
}