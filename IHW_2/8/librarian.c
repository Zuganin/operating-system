#include "common.h"
#include <stdio.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>

shm_t *shm;
int semid;

void sem_op(int sem, int op) {
    struct sembuf s = {sem, op, 0};
    semop(semid, &s, 1);
}

void safe_wait(int sem) { sem_op(sem, -1); }
void safe_post(int sem) { sem_op(sem, 1); }

int main() {
    // Получаем идентификатор разделяемой памяти и семафоров
    int shmid = shmget(SHM_KEY, sizeof(shm_t), 0666);
    if (shmid == -1) {
        perror("shmget failed");
        return 1;
    }
    
    shm = shmat(shmid, NULL, 0);
    if (shm == (shm_t *) -1) {
        perror("shmat failed");
        return 1;
    }

    semid = semget(SEM_KEY, SEM_COUNT, 0666);
    if (semid == -1) {
        perror("semget failed");
        return 1;
    }

    // Бесконечный цикл для обработки задач библиотекаря
    while (1) {
        safe_wait(SEM_NOTIFY);
        safe_wait(SEM_MUTEX_QUEUE);
        int idx = shm->queue[shm->head];
        shm->head = (shm->head + 1) % MAXQ;
        int done = (idx == -1 && shm->readers_done == M);
        safe_post(SEM_MUTEX_QUEUE);

        if (done) {
            printf("[Библиотекарь] Все читатели завершили работу. Завершаюсь.\n");
            break;
        }

        if (idx >= 0) {
            printf("[\x1B[34mБиблиотекарь\x1B[0m] уведомляю читателя о книге %d\n", idx);
            fflush(stdout);
            safe_post(SEM_BOOK_SEM_START + idx);
        }
    }

    // Отключаем разделяемую память и завершаем процесс
    shmdt(shm);
    return 0;
}
