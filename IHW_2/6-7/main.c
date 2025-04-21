#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>

#define SHM_NAME      "/lib_shm"
#define N   3  // число книг
#define M   2  // число читателей
#define MAXQ 10000

typedef struct {
    int head, tail;
    int queue[MAXQ];
    int books[N];
    int done;
    sem_t mutex_queue;
    sem_t notify;
    sem_t book_mutex[N];
    sem_t book_sem[N];
} shm_t;

static shm_t *shm = NULL;
static int shm_fd = -1;

void safe_sem_post(sem_t *s) {
    while (sem_post(s) == -1 && errno == EINTR);
}

void safe_sem_wait(sem_t *s) {
    while (sem_wait(s) == -1 && errno == EINTR);
}

void cleanup_and_exit(int code) {
    if (shm) munmap(shm, sizeof(shm_t));
    if (shm_fd != -1) {
        close(shm_fd);
        shm_unlink(SHM_NAME);
    }
    exit(code);
}

void sigint_handler(int sig) {
    cleanup_and_exit(0);
}

void librarian_proc() {
    while (1) {
        safe_sem_wait(&shm->notify);
        safe_sem_wait(&shm->mutex_queue);
        int idx = shm->queue[shm->head];
        shm->head = (shm->head + 1) % MAXQ;
        safe_sem_post(&shm->mutex_queue);

        if (idx < 0 && shm->done) break;

        printf("[\x1B[34mБиблиотекарь\x1B[0m] уведомляю читателя о книге %d\n", idx);
        fflush(stdout);
        safe_sem_post(&shm->book_sem[idx]);
    }
    cleanup_and_exit(0);
}

void reader_proc(int id) {
    srand(time(NULL) ^ getpid());
    int k = 1 + rand() % 3;
    printf("[\x1B[32mЧитатель %d\x1B[0m] хочет прочитать %d книг(-и)\n", id, k);
    int wants[3] = {-1, -1, -1}, cnt = 0;

    while (cnt < k) {
        int x = rand() % N, dup = 0;
        for (int i = 0; i < cnt; ++i)
            if (wants[i] == x) dup = 1;
        if (!dup) wants[cnt++] = x;
    }

    for (int i = 0; i < cnt; ++i) {
        int b = wants[i];
        while (1) {
            safe_sem_wait(&shm->book_mutex[b]);
            if (shm->books[b] == 1) {
                shm->books[b] = 0;
                safe_sem_post(&shm->book_mutex[b]);

                printf("[Читатель %d] взял книгу %d\n", id, b);
                fflush(stdout);

                int t = 1 + rand() % 2;
                printf("[Читатель %d] читает книгу %d %d дней\n", id, b, t);
                fflush(stdout);
                sleep(t);

                safe_sem_wait(&shm->book_mutex[b]);
                shm->books[b] = 1;
                safe_sem_post(&shm->book_mutex[b]);

                safe_sem_wait(&shm->mutex_queue);
                shm->queue[shm->tail] = b;
                shm->tail = (shm->tail + 1) % MAXQ;
                safe_sem_post(&shm->mutex_queue);

                safe_sem_post(&shm->notify);
                printf("[Читатель %d] вернул книгу %d\n", id, b);
                fflush(stdout);
                sleep(10);
                break;
            } else {
                safe_sem_post(&shm->book_mutex[b]);
                printf("[Читатель %d] ожидает книгу %d\n", id, b);
                fflush(stdout);
                safe_sem_wait(&shm->book_sem[b]);
            }
        }
    }
    cleanup_and_exit(0);
}

int main() {
    signal(SIGINT, sigint_handler);

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(shm_t));
    shm = mmap(NULL, sizeof(shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    memset(shm, 0, sizeof(shm_t));

    for (int i = 0; i < N; i++) shm->books[i] = 1;

    sem_init(&shm->mutex_queue, 1, 1);
    sem_init(&shm->notify, 1, 0);
    for (int i = 0; i < N; i++) {
        sem_init(&shm->book_mutex[i], 1, 1);
        sem_init(&shm->book_sem[i], 1, 0);
    }

    pid_t lib_pid = fork();
    if (lib_pid == 0) librarian_proc();

    pid_t readers[M];
    for (int i = 0; i < M; i++) {
        if ((readers[i] = fork()) == 0) reader_proc(i + 1);
    }

    for (int i = 0; i < M; i++) waitpid(readers[i], NULL, 0);

    shm->done = 1;
    safe_sem_wait(&shm->mutex_queue);
    shm->queue[shm->tail] = -1;
    shm->tail = (shm->tail + 1) % MAXQ;
    safe_sem_post(&shm->mutex_queue);
    safe_sem_post(&shm->notify);

    waitpid(lib_pid, NULL, 0);
    cleanup_and_exit(0);
}
