#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for recv() and send() */
#include <unistd.h>     /* for close() */
#include <string.h>     /* for memset() */
#include <stdlib.h>     /* for rand() */
#include <time.h>       /* для sleep() */
#include <stdatomic.h>    /* для atomic_int */
#include "HandleTCPMonitor.h"

#define RCVBUFSIZE 1024   /* Размер буфера приема */

extern int servSock;   /* Серверный сокет */
extern int winnieFound; /* Глобальный флаг, найден ли Винни-Пух */
void DieWithError(char *errorMessage);  /* Функция обработки ошибок */

void HandleTCPClient(int clntSocket, int *forest, int forestSize,  int winnieRegion, int *checkedRegions)
{
    char buffer[RCVBUFSIZE];         /* Буфер для сообщений */
    int recvMsgSize;                 /* Размер полученного сообщения */
    int swarmID = 0;                 /* ID стаи пчел */
    char responseBuffer[RCVBUFSIZE]; /* Буфер для ответа */
    
    /* Получаем первое сообщение от клиента - инициализация стаи */
    if ((recvMsgSize = recv(clntSocket, buffer, RCVBUFSIZE, 0)) < 0)
        DieWithError("Ошибка при получении сообщения");
    
    buffer[recvMsgSize] = '\0';
    
    /* Проверяем, является ли клиент монитором */
    if (IsMonitor(buffer)) {
        /* Добавляем монитор в список мониторов */
        AddMonitor(clntSocket);
        
        /* Не закрываем сокет монитора и выходим из функции */
        return;
    }
    
    /* Это обычный клиент - стая пчел. Извлекаем ID стаи из сообщения */
    if (sscanf(buffer, "INIT %d", &swarmID) != 1) {
        DieWithError("Недопустимое сообщение инициализации");
    }
    
    printf("[Обработчик] ПОДКЛЮЧЕНИЕ: Стая пчел #%d инициализирована\n", swarmID);
    
    /* Отправка информации о подключении стаи пчел мониторам */
    char monitorMsg[RCVBUFSIZE];
    sprintf(monitorMsg, "Стая пчел #%d инициализирована", swarmID);
    SendToMonitors(monitorMsg);
    
    /* Определение того, завершен ли поиск */
    if (winnieFound) {
        /* Если Винни-Пух уже найден, сообщаем клиенту */
        sprintf(responseBuffer, "DONE Винни-Пух уже найден и наказан");
        if (send(clntSocket, responseBuffer, strlen(responseBuffer), 0) != strlen(responseBuffer))
            DieWithError("Ошибка при отправке сообщения");
        
        printf("[Обработчик] ИНФОРМАЦИЯ: Сообщаем стае #%d, что Винни-Пух уже найден\n", swarmID);
        
        /* Отправка информации мониторам */
        sprintf(monitorMsg, "Сообщаем стае #%d, что Винни-Пух уже найден", swarmID);
        SendToMonitors(monitorMsg);
        
        close(clntSocket);
        return;
    }
    
    /* Основной цикл обработки запросов от клиента */
    while (1) {
        // ... выбор региона ...
        int regionToCheck = -1;
        for (int i = 0; i < forestSize; ++i) {
            int idx = (rand() + i) % forestSize;
            if (checkedRegions[idx] == 0) {
                regionToCheck = idx;
                checkedRegions[idx] = 1;
                break;
            }
        }
        if (regionToCheck == -1) {
            break;
        }

        // Отправка региона клиенту
        sprintf(responseBuffer, "CHECK %d %d", regionToCheck, forest[regionToCheck]);
        if (send(clntSocket, responseBuffer, strlen(responseBuffer), 0) != strlen(responseBuffer))
            DieWithError("Ошибка при отправке сообщения");
            
        /* Отправка информации мониторам */
        char monitorMsg[RCVBUFSIZE];
        sprintf(monitorMsg, "Стае #%d отправлен запрос на проверку региона %d", 
                swarmID, regionToCheck);
        SendToMonitors(monitorMsg);

        // Получение ответа от клиента
        if ((recvMsgSize = recv(clntSocket, buffer, RCVBUFSIZE, 0)) < 0) {
            if (winnieFound == 0) {
                printf("[Обработчик] ОПОВЕЩЕНИЕ: Сервер завершил работу по сигналу\n");
            } else {
                DieWithError("Ошибка при получении сообщения");
            }
            break;
        }
            
        
        if (recvMsgSize == 0) {
            printf("[Обработчик] ОТКЛЮЧЕНИЕ: Стая пчел #%d отключилась\n", swarmID);
            /* Отправка информации мониторам */
            char monitorMsg[RCVBUFSIZE];
            sprintf(monitorMsg, "Стая пчел #%d отключилась", swarmID);
            SendToMonitors(monitorMsg);
            break;
        }

        buffer[recvMsgSize] = '\0';

        if (strncmp(buffer, "FOUND", 5) == 0) {
            winnieFound = 1;  // Устанавливаем глобальный флаг, что Винни-Пух найден
            printf("[Обработчик] УСПЕХ: Стая пчел #%d сообщает: Винни-Пух найден в регионе %d и наказан!\n", 
                   swarmID, regionToCheck);
            
            /* Отправка информации мониторам */
            char monitorMsg[RCVBUFSIZE];
            sprintf(monitorMsg, "УСПЕХ: Стая пчел #%d сообщает: Винни-Пух найден в регионе %d и наказан!", 
                    swarmID, regionToCheck);
            SendToMonitors(monitorMsg);
            
            sprintf(responseBuffer, "CONFIRM Винни-Пух наказан");
            send(clntSocket, responseBuffer, strlen(responseBuffer), 0);
            
            // Закрываем сокет сервера, что приведет к разрыву всех соединений
            close(servSock);
            
            // Завершаем работу сервера
            printf("[Обработчик] ЗАВЕРШЕНИЕ: Винни-Пух найден, завершаем работу сервера\n");
            exit(0);
            break;
        } else if (strncmp(buffer, "NOTFOUND", 8) == 0) {
            if (winnieFound == 1) {
                // Сообщаем клиенту, что Винни-Пух найден другой стаей и завершаем соединение
                sprintf(responseBuffer, "DONE Винни-Пух найден другой стаей");
                send(clntSocket, responseBuffer, strlen(responseBuffer), 0);
                printf("[Обработчик] ОПОВЕЩЕНИЕ: Стае #%d сообщено, что Винни-Пух найден другой стаей\n", swarmID);
                
                /* Отправка информации мониторам */
                char monitorMsg[RCVBUFSIZE];
                sprintf(monitorMsg, "Стае #%d сообщено, что Винни-Пух найден другой стаей", swarmID);
                SendToMonitors(monitorMsg);
                
                break;
            }
            printf("[Обработчик] ПОИСК: Стая пчел #%d сообщает: Регион %d чист\n", swarmID, regionToCheck);
            
            /* Отправка информации мониторам */
            char monitorMsg[RCVBUFSIZE];
            sprintf(monitorMsg, "Стая пчел #%d сообщает: Регион %d чист", swarmID, regionToCheck);
            SendToMonitors(monitorMsg);
            
        } else if (strncmp(buffer, "DONE", 4) == 0) {
            printf("[Обработчик] ИНФОРМАЦИЯ: %s\n", buffer + 5);
            
            /* Отправка информации мониторам */
            char monitorMsg[RCVBUFSIZE];
            sprintf(monitorMsg, "ИНФОРМАЦИЯ: %s", buffer + 5);
            SendToMonitors(monitorMsg);
            
            break;
        } else if (strncmp(buffer, "FINISH", 6) == 0) {
            printf("[Обработчик] ИНФОРМАЦИЯ: %s\n", buffer + 7);
            
            /* Отправка информации мониторам */
            char monitorMsg[RCVBUFSIZE];
            sprintf(monitorMsg, "ИНФОРМАЦИЯ: %s", buffer + 7);
            SendToMonitors(monitorMsg);
            
            break;
        }
        else {
            printf("[Обработчик] ПРЕДУПРЕЖДЕНИЕ: Получено неизвестное сообщение от стаи #%d: %s\n", swarmID, buffer);
            
            /* Отправка информации мониторам */
            char monitorMsg[RCVBUFSIZE];
            sprintf(monitorMsg, "ПРЕДУПРЕЖДЕНИЕ: Получено неизвестное сообщение от стаи #%d: %s", swarmID, buffer);
            SendToMonitors(monitorMsg);
        }
    }   
    sleep(3); // Задержка для завершения работы клиента
    close(clntSocket);    /* Закрываем сокет клиента */
}
