#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>

#define name "/time"

struct shared_data {
    sem_t sem;
    char time_str[256];
};

int main() {
    int shm_fd = shm_open(name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    struct shared_data *data = mmap(NULL, sizeof(struct shared_data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    while (1) {
        if (sem_wait(&data->sem) == -1) {
            perror("sem_wait");
            break;
        }
        printf("Current time: %s\n", data->time_str);
    }

    munmap(data, sizeof(struct shared_data));
    close(shm_fd);
    return EXIT_SUCCESS;
}