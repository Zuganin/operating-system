#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdatomic.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>



// Проверил, запустив несколько процессов в разных терминалах, и они корректно увеличивают счётчик в общей памяти.

volatile sig_atomic_t finish_flag = 0;
atomic_int *counter = NULL;
int shmid = -1;
bool remove_flag = false;

void handler(int sig) {
    finish_flag = 1;
    printf("Process %d завершает работу по сигналу...\n", getpid());
}

int main() {

    signal(SIGINT, handler);
    key_t key = ftok(".", 52);
    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    shmid = shmget(key, sizeof(atomic_int), IPC_CREAT | IPC_EXCL | 0600);
    if (shmid == -1) {
        if (errno == EEXIST) {
            shmid = shmget(key, sizeof(atomic_int), 0);
            if (shmid == -1) {
                perror("shmget (существующий)");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("shmget (создание)");
            exit(EXIT_FAILURE);
        }
    } else {
        remove_flag = true;
    }

    counter = (atomic_int *)shmat(shmid, NULL, 0);
    if (counter == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    if (remove_flag) {
        atomic_store(counter, 0);
    }

    while (!finish_flag) {
        int value = atomic_fetch_add(counter, 1);
        printf("PID %d: %d\n", getpid(), value);
        usleep(400000);
    }

    if (shmdt((void *)counter) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    if (remove_flag) {
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl(IPC_RMID)");
            exit(EXIT_FAILURE);
        }
        printf("Сегмент памяти удалён.\n");
    }

    printf("Процесс %d завершил работу.\n", getpid());
    return 0;
}
