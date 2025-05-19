#ifndef HANDLE_TCP_MONITOR_H
#define HANDLE_TCP_MONITOR_H

#define MAXMONITORS 10   /* Максимальное количество мониторов */

/* Внешние переменные */
extern int monitorSockets[MAXMONITORS]; /* Массив для хранения дескрипторов сокетов мониторов */
extern int monitorCount;               /* Количество мониторов */

/* Функция для проверки, является ли клиент монитором */
int IsMonitor(char *message);

/* Функция для добавления нового монитора */
void AddMonitor(int socket);

/* Функция для удаления монитора (при отключении) */
void RemoveMonitor(int socket);

/* Функция для отправки данных всем мониторам */
void SendToMonitors(const char *message);

#endif /* HANDLE_TCP_MONITOR_H */
