#ifndef COMMON_H
#define COMMON_H

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define N 3 // количество книг
#define M 3 // количество читателей

#define MAXQ 10000

typedef struct {
    int head, tail;
    int queue[MAXQ];
    int books[N];
    int done;
    int readers_done;
} shm_t;

enum {
    SEM_MUTEX_QUEUE = 0,
    SEM_NOTIFY,
    SEM_BOOK_MUTEX_START,   // N штук далее
    SEM_BOOK_SEM_START = SEM_BOOK_MUTEX_START + N,
    SEM_COUNT = SEM_BOOK_SEM_START + N
};

#endif
