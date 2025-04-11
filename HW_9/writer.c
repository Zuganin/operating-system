#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <time.h>
#include <signal.h>

#define name "/time"

struct shared_data {
    sem_t sem;
    char time_str[256];
};

volatile sig_atomic_t stop = 0;

void handle_signal(int sig) {
    stop = 1;
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, sizeof(struct shared_data)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    struct shared_data *data = mmap(NULL, sizeof(struct shared_data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&data->sem, 1, 0) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    while (!stop) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        strftime(data->time_str, sizeof(data->time_str), "%Y-%m-%d %H:%M:%S", tm);

        if (sem_post(&data->sem) == -1) {
            perror("sem_post");
            break;
        }

        sleep(1);
    }

    sem_destroy(&data->sem);
    munmap(data, sizeof(struct shared_data));
    close(shm_fd);
    shm_unlink(name);
    return EXIT_SUCCESS;
}