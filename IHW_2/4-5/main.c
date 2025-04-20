
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
#define SEM_MUTEX_Q   "/sem_mutex_queue"
#define SEM_NOTIFY    "/sem_notify"
#define SEM_BOOK_FMT  "/sem_book_%d"
#define SEM_BOOK_MUTEX_FMT "/sem_book_mutex_%d"

#define N   3  // число книг
#define M   2  // число читателей
#define MAXQ 10000

typedef struct {
    int head, tail;
    int queue[MAXQ];
    int books[N];
    int done;
} shm_t;

static shm_t *shm = NULL;
static int shm_fd = -1;
static sem_t *sem_mutex_queue = NULL;
static sem_t *sem_notify = NULL;
static sem_t *sem_books[N];
static sem_t *sem_book_mutex[N];

void safe_sem_post(sem_t *s) {
    while (sem_post(s) == -1 && errno == EINTR);
}


void cleanup_and_exit(int code) {
    if (sem_mutex_queue) {
        sem_close(sem_mutex_queue);
        sem_unlink(SEM_MUTEX_Q);
    }
    if (sem_notify) {
        sem_close(sem_notify);
        sem_unlink(SEM_NOTIFY);
    }
    for (int i = 0; i < N; i++) {
        char name[64];
        if (sem_books[i]) sem_close(sem_books[i]);
        snprintf(name, sizeof(name), SEM_BOOK_FMT, i);
        sem_unlink(name);
        if (sem_book_mutex[i]) sem_close(sem_book_mutex[i]);
        snprintf(name, sizeof(name), SEM_BOOK_MUTEX_FMT, i);
        sem_unlink(name);
    }
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

void safe_sem_wait(sem_t *s) {
    while (sem_wait(s) == -1 && errno == EINTR);
}

void librarian_proc() {
    while (1) {
        safe_sem_wait(sem_notify);
        safe_sem_wait(sem_mutex_queue);
        int idx = shm->queue[shm->head];
        shm->head = (shm->head + 1) % MAXQ;
        safe_sem_post(sem_mutex_queue);

        if (idx < 0 && shm->done) break;

        printf("[Библиотекарь] уведомляю читателя о книге %d\n", idx);
        fflush(stdout);
        sem_post(sem_books[idx]);
    }
    cleanup_and_exit(0);
}

void reader_proc(int id) {
    srand(time(NULL) ^ getpid());
    int k = 1 + rand() % 3; // сколько книг хочет получить
    printf("[Читатель %d] хочет прочитать %d книг(-и)\n", id, k);
    int wants[3] = {-1,-1,-1}, cnt = 0;

    while (cnt < k) {
        int x = rand() % N, dup = 0;
        for (int i = 0; i < cnt; ++i)
            if (wants[i] == x) dup = 1;
        if (!dup) wants[cnt++] = x;
    }

    for (int i = 0; i < cnt; ++i) {
        int b = wants[i];

        while (1) {
            safe_sem_wait(sem_book_mutex[b]);
            if (shm->books[b] == 1) {
                shm->books[b] = 0;
                sem_post(sem_book_mutex[b]);

                printf("[Читатель %d] взял книгу %d\n", id, b);
                fflush(stdout);

                int t = 1 + rand() % 2;
                printf("[Читатель %d] читает книгу %d %d дней\n", id, b, t);
                fflush(stdout);
                sleep(t);

                // возвращаем книгу
                safe_sem_wait(sem_book_mutex[b]);
                shm->books[b] = 1;
                sem_post(sem_book_mutex[b]);

                safe_sem_wait(sem_mutex_queue);
                shm->queue[shm->tail] = b;
                shm->tail = (shm->tail + 1) % MAXQ;
                sem_post(sem_mutex_queue);

                sem_post(sem_notify);
                printf("[Читатель %d] вернул книгу %d\n", id, b);
                fflush(stdout);

                sleep(10); // пауза перед следующим запросом
                break;

            } else {
                sem_post(sem_book_mutex[b]);
                printf("[Читатель %d] ожидает книгу %d\n", id, b);
                fflush(stdout);
                safe_sem_wait(sem_books[b]); // дождаться сигнала от библиотекаря
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

    sem_mutex_queue = sem_open(SEM_MUTEX_Q, O_CREAT, 0666, 1);
    sem_notify = sem_open(SEM_NOTIFY, O_CREAT, 0666, 0);
    for (int i = 0; i < N; i++) {
        char name[64];
        snprintf(name, sizeof(name), SEM_BOOK_FMT, i);
        sem_books[i] = sem_open(name, O_CREAT, 0666, 0);
        snprintf(name, sizeof(name), SEM_BOOK_MUTEX_FMT, i);
        sem_book_mutex[i] = sem_open(name, O_CREAT, 0666, 1);
    }

    pid_t lib_pid = fork();
    if (lib_pid == 0) librarian_proc();

    pid_t readers[M];
    for (int i = 0; i < M; i++) {
        if ((readers[i] = fork()) == 0) reader_proc(i + 1);
    }

    for (int i = 0; i < M; i++) waitpid(readers[i], NULL, 0);

    shm->done = 1;
    safe_sem_wait(sem_mutex_queue);
    shm->queue[shm->tail] = -1;
    shm->tail = (shm->tail + 1) % MAXQ;
    sem_post(sem_mutex_queue);
    sem_post(sem_notify);

    waitpid(lib_pid, NULL, 0);
    cleanup_and_exit(0);
}
