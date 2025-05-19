# Компилятор
CC = gcc

# Флаги компиляции
CFLAGS = -Wall -g

# Флаги компоновки
LDFLAGS = -lpthread

# Цели
all: server client monitor

# Сборка сервера с разделенными файлами
server: server.c HandleTCPClient.c DieWithError.c HandleTCPMonitor.c
	$(CC) $(CFLAGS) -o server server.c HandleTCPClient.c DieWithError.c HandleTCPMonitor.c $(LDFLAGS)

# Сборка клиента
client: client.c DieWithError.c
	$(CC) $(CFLAGS) -o client client.c DieWithError.c

# Сборка монитора
monitor: monitor.c DieWithError.c
	$(CC) $(CFLAGS) -o monitor monitor.c DieWithError.c

# Очистка собранных файлов
clean:
	rm -f server client monitor *.o

# Справка по запуску
help:
	@echo "Для запуска сервера используйте команду:"
	@echo "  ./server <IP-адрес> <порт> <размер-леса> <номер-региона-с-Винни>"
	@echo "Пример: ./server 127.0.0.1 5555 100 52"
	@echo ""
	@echo "Для запуска клиента используйте команду:"
	@echo "  ./client <IP-адрес-сервера> <порт-сервера>"
	@echo "Пример: ./client 127.0.0.1 5555"
	@echo ""
	@echo "Для запуска монитора используйте команду:"
	@echo "  ./monitor <IP-адрес-сервера> <порт-сервера>"
	@echo "Пример: ./monitor 127.0.0.1 5555"

.PHONY: all clean help
