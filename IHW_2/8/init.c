#include "common.h"
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <string.h>

int main() {
    int shmid = shmget(SHM_KEY, sizeof(shm_t), IPC_CREAT | 0666);
    shm_t *shm = shmat(shmid, NULL, 0);
    memset(shm, 0, sizeof(shm_t));
    for (int i = 0; i < N; ++i) shm->books[i] = 1;

    shm->done = 0;
    shm->readers_done = 0;
    int semid = semget(SEM_KEY, SEM_COUNT, IPC_CREAT | 0666);
    semctl(semid, SEM_MUTEX_QUEUE, SETVAL, 1);
    semctl(semid, SEM_NOTIFY, SETVAL, 0);
    for (int i = 0; i < N; ++i) {
        semctl(semid, SEM_BOOK_MUTEX_START + i, SETVAL, 1);
        semctl(semid, SEM_BOOK_SEM_START + i, SETVAL, 0);
    }

    printf("Инициализации прошла успешно!\n");
    return 0;
}
