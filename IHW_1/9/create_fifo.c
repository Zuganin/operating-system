
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>


const char *fifo1 = "fifo1";
const char *fifo2 = "fifo2";

void clean() {
    unlink(fifo1);
    unlink(fifo2);
  }
  

int main(){
    if (mkfifo(fifo1, 0666) < 0 || mkfifo(fifo2, 0666) < 0) {
        perror("mkfifo error");
        clean();
        exit(1);
    }
}