#include <stdio.h>
#include <string.h>     
#include <unistd.h>
#include <sys/socket.h> /* Для функции send() */
#include <errno.h>      /* Для обработки ошибок сокетов */
#include "HandleTCPMonitor.h"

/* Определение флага MSG_NOSIGNAL для macOS, если он не определен */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define MAXMONITORS 10   /* Максимальное количество мониторов */
#define BUFSIZE 1024      /* Размер буфера */

/* Массив для хранения дескрипторов сокетов мониторов */
int monitorSockets[MAXMONITORS];
int monitorCount = 0;

/* Функция для проверки, является ли клиент монитором */
int IsMonitor(char *message) {
    /* Проверяем, содержит ли сообщение строку "MONITOR" */
    return (strncmp(message, "MONITOR", 7) == 0);
}

/* Функция для добавления нового монитора */
void AddMonitor(int socket) {
    if (monitorCount < MAXMONITORS) {
        monitorSockets[monitorCount++] = socket;
        printf("[Сервер] МОНИТОРИНГ: Новый монитор подключен (сокет: %d)\n", socket);
    } else {
        printf("[Сервер] ПРЕДУПРЕЖДЕНИЕ: Достигнуто максимальное количество мониторов (%d)\n", MAXMONITORS);
        close(socket);  /* Закрываем сокет, если достигнут лимит мониторов */
    }
}

/* Функция для удаления монитора (при отключении) */
void RemoveMonitor(int socket) {
    for (int i = 0; i < monitorCount; i++) {
        if (monitorSockets[i] == socket) {
            /* Сдвигаем все элементы после удаляемого */
            for (int j = i; j < monitorCount - 1; j++) {
                monitorSockets[j] = monitorSockets[j + 1];
            }
            monitorCount--;
            printf("[Сервер] МОНИТОРИНГ: Монитор отключен (сокет: %d)\n", socket);
            break;
        }
    }
}

/* Функция для проверки, что сообщение не связано с мониторами */
static int IsMonitorRelatedMessage(const char *message) {
    return (strstr(message, "монитор") != NULL || 
            strstr(message, "Монитор") != NULL);
}

/* Функция для отправки данных всем мониторам */
void SendToMonitors(const char *message) {
    char monitorMessage[BUFSIZE];
    
    /* Пропускаем сообщения, связанные с мониторами */
    if (IsMonitorRelatedMessage(message)) {
        return;
    }
    
    /* Добавляем префикс [Монитор] к сообщению и символ перевода строки для лучшей читаемости */
    snprintf(monitorMessage, BUFSIZE, "[Монитор] %s", message);
    
    /* Отправляем сообщение всем мониторам */
    for (int i = 0; i < monitorCount; i++) {
        /* Проверяем состояние сокета и отправляем сообщение */
        int result = send(monitorSockets[i], monitorMessage, strlen(monitorMessage), MSG_NOSIGNAL);
        if (result <= 0) {
            /* Если отправка не удалась, значит монитор отключился */
            printf("[Сервер] МОНИТОРИНГ: Монитор отключен (сокет: %d) - обнаружено при отправке сообщения\n", monitorSockets[i]);
            /* Удаляем монитор из списка */
            RemoveMonitor(monitorSockets[i]);
            i--; /* Уменьшаем счетчик, так как размер массива уменьшился */
        }
    }
}