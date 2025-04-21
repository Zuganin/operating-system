#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

shm_t *shm;
int semid;

void sem_op(int sem, int op) {
    struct sembuf s = {sem, op, 0};
    semop(semid, &s, 1);
}

void safe_wait(int sem) { sem_op(sem, -1); }
void safe_post(int sem) { sem_op(sem, 1); }

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <reader_id>\n", argv[0]);
        return 1;
    }
    int id = atoi(argv[1]);

    int shmid = shmget(SHM_KEY, sizeof(shm_t), 0666);
    shm = shmat(shmid, NULL, 0);
    semid = semget(SEM_KEY, SEM_COUNT, 0666);

    srand(time(NULL) ^ getpid());
    int k = 1 + rand() % 3;
    printf("[\x1B[32mЧитатель %d\x1B[0m] хочет прочитать %d книг(-и)\n", id, k);
    int wants[3], cnt = 0;

    while (cnt < k) {
        int x = rand() % N, ok = 1;
        for (int i = 0; i < cnt; i++)
            if (wants[i] == x) ok = 0;
        if (ok) wants[cnt++] = x;
    }

    for (int i = 0; i < cnt; i++) {
        int b = wants[i];
        while (1) {
            safe_wait(SEM_BOOK_MUTEX_START + b);
            if (shm->books[b] == 1) {
                shm->books[b] = 0;
                safe_post(SEM_BOOK_MUTEX_START + b);
                printf("[Читатель %d] взял книгу %d\n", id, b);

                int t = 1 + rand() % 2;
                printf("[Читатель %d] читает книгу %d %d дней\n", id, b, t);
                fflush(stdout);
                sleep(t);

                safe_wait(SEM_BOOK_MUTEX_START + b);
                shm->books[b] = 1;
                safe_post(SEM_BOOK_MUTEX_START + b);

                safe_wait(SEM_MUTEX_QUEUE);
                shm->queue[shm->tail] = b;
                shm->tail = (shm->tail + 1) % MAXQ;
                safe_post(SEM_MUTEX_QUEUE);

                safe_post(SEM_NOTIFY);
                printf("[Читатель %d] вернул книгу %d\n", id, b);
                fflush(stdout);
                sleep(10);
                break;
            } else {
                safe_post(SEM_BOOK_MUTEX_START + b);
                printf("[Читатель %d] ожидает книгу %d\n", id, b);
                safe_wait(SEM_BOOK_SEM_START + b);
            }
        }
    }
    safe_wait(SEM_MUTEX_QUEUE);
    shm->readers_done++;
    printf("[\x1B[31mЧитатель \x1B[0m %d] завершил чтение\n", id);
    fflush(stdout);
    int all_done = (shm->readers_done == M); // NUM_READERS можно тоже хранить в shm
    shm->queue[shm->tail] = -1;
    shm->tail = (shm->tail + 1) % MAXQ;
    safe_post(SEM_MUTEX_QUEUE);
    safe_post(SEM_NOTIFY);

    if (all_done) {
        printf("[Читатель %d] Последний читатель — сигнал библиотекарю завершиться\n", id);
    }

    

    shmdt(shm);
    return 0;
}
