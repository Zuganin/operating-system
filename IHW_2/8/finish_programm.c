#include "common.h"
#include <stdio.h>
#include <sys/shm.h>

shm_t *shm;

int main() {
    int shmid = shmget(SHM_KEY, sizeof(shm_t), 0666);
    shm = shmat(shmid, NULL, 0);

    // Устанавливаем флаг завершения
    shm->done = 1;
    printf("[\x1B[34mБиблиотекарь\x1B[0m] Завершаю свою работу, все читатели завершили.\n");
    fflush(stdout);

    shmdt(shm);
    return 0;
}
