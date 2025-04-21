#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <errno.h>
#include <string.h>

//

#define SHM_NAME "/counter"  

volatile sig_atomic_t finish_flag = 0;
atomic_int *counter = NULL;
int shm_fd = -1;  


void handler(int sig) {
    finish_flag = 1;
    printf("Процесс %d завершает работу по сигналу...\n", getpid());
}

int main() {
    struct sigaction finish = {.sa_handler = &handler};
    if (sigaction(SIGINT, &finish, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    shm_fd = shm_open(name, O_CREAT | O_RDWR, 0600);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }

    if (ftruncate(shm_fd, sizeof(atomic_int)) == -1) {
        perror("ftruncate");
        return 1;
    }

    counter = (atomic_int *)mmap(NULL, sizeof(atomic_int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (counter == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    if (atomic_load(counter) == 0) {
        atomic_store(counter, 0);
    }

    while (!finish_flag) {
        int value = atomic_fetch_add(counter, 1); 
        printf("PID %d: %d\n", getpid(), value);
        usleep(400000);  
    }

    if (munmap(counter, sizeof(atomic_int)) == -1) {
        perror("munmap");
        return 1;
    }

    if (finish_flag) {
        if (shm_unlink(name) == -1) {
            perror("shm_unlink");
            return 1;
        }
        printf("Сегмент памяти удалён.\n");
    }

    return 0;
}
