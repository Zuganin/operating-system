#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 5000
const char *fifo1 = "fifo1";
const char *fifo2 = "fifo2";

void clean() {
  unlink(fifo1);
  unlink(fifo2);
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        char *usage = "Usage: ./program <input_file> <output_file>\n";
        write(STDOUT_FILENO, usage, strlen(usage));
        exit(1);
    }

    int fd_in = open(argv[1], O_RDONLY);
    if (fd_in < 0) {
        perror("open input file"); 
        clean();
        exit(1);
    }

    int fifo1_wr = open(fifo1, O_WRONLY);
    if (fifo1_wr < 0) {
        perror("open fifo1 for writing");
        close(fd_in);
        clean();
        exit(1);
    }
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = read(fd_in, buffer, BUFFER_SIZE)) > 0) {
        if (write(fifo1_wr, buffer, bytes_read) != bytes_read) {
            perror("write to fifo1");
            close(fd_in);
            close(fifo1_wr);
            clean();
            exit(1);
        }
    }

    close(fd_in);
    close(fifo1_wr); 
    
    sleep(1);

    int fifo2_rd = open(fifo2, O_RDONLY);
    if (fifo2_rd < 0) {
        perror("open fifo2 for reading");
        clean();
        exit(1);
    }

    char result[BUFFER_SIZE];
    int bytes_result = read(fifo2_rd, result, BUFFER_SIZE);
    
    close(fifo2_rd);

    if (bytes_result <= 0) {
        perror("read from fifo2");
        clean();
        exit(1);
    }

    int fd_out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        perror("open output file");
        wait(NULL);
        clean();
        exit(1);
    }

    if (write(fd_out, result, bytes_result) != bytes_result) {
        perror("write to output file");
    }

    close(fd_out);
    clean();

    return 0;
}