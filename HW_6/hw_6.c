#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>

int fd;
char *log_file = "logs-1";

void switch_log(int signum) {
    const char *msg = "Получили SIGUSR1. Переключаем файл...\n";
    write(1, msg, strlen(msg));
    if (fd >= 0) {
        close(fd);
    }
    log_file = (strcmp(log_file, "logs-1") == 0) ? "logs-2" : "logs-1";
    fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        const char *err_msg = "Ошибка открытия файла\n";
        write(1, err_msg, strlen(err_msg));
        exit(1);
    }
    char new_log_msg[256];
    snprintf(new_log_msg, sizeof(new_log_msg), "Запись теперь ведется в файл: %s\n", log_file);
    write(1, new_log_msg, strlen(new_log_msg)); 
}

void my_exit(int signum) {
    const char *msg = "\nПользовательн нажал ctrl + C (сигнал SIGINT). Завершаем программу...\n";
    write(1, msg, strlen(msg));
    if (fd >= 0){
        close(fd);
    }
    exit(0);
}

int main() {
    struct sigaction sa_int, sa_usr1;

    sa_int.sa_handler = my_exit;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    sa_usr1.sa_handler = switch_log;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Ошибка открытия файла");
        exit(1);
    }

    const char *start_msg = "Начало записи логов в файл: logs-1\n";
    write(1, start_msg, strlen(start_msg));

    while (1) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", t);
        dprintf(fd, "%s\n", buffer);
        fsync(fd);
        sleep(1);
    }

    return 0;
}
