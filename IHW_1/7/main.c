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


int process_text(char *buffer, int size, int *in_word, int *word_started_with_letter) {
    int count = 0;
    for (int i = 0; i < size; i++) {
        if (!isalnum((unsigned char)buffer[i])) {
            if (*in_word) {
                if (*word_started_with_letter) {
                    count++;
                }
                *in_word = 0;
                *word_started_with_letter = 0;
            }
        } else {
            if (!(*in_word)) {
                *in_word = 1;

                if (isalpha((unsigned char)buffer[i])) {
                    *word_started_with_letter = 1;
                } else {
                    *word_started_with_letter = 0;
                }
            }
        }
    }
    return count;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        char *usage = "Usage: ./program <input_file> <output_file>\n";
        write(STDOUT_FILENO, usage, strlen(usage));
        exit(1);
    }

    clean();

    if (mkfifo(fifo1, 0666) < 0 || mkfifo(fifo2, 0666) < 0) {
        perror("mkfifo error");
        clean();
        exit(1);
    }

    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork");
        clean();
        exit(1);
    }

    if (pid1 == 0) { 
        
        int fifo1_rd = open(fifo1, O_RDONLY);
        if (fifo1_rd < 0) {
            perror("open fifo1 for reading");
            exit(1);
        }

        char buffer[BUFFER_SIZE];
        int bytes_read;
        int count = 0;
        int in_word = 0;
        int word_started_with_letter = 0;

        while ((bytes_read = read(fifo1_rd, buffer, BUFFER_SIZE)) > 0) {
            count += process_text(buffer, bytes_read, &in_word, &word_started_with_letter);
        }
        
        if (in_word && word_started_with_letter) {
            count++;
        }
        close(fifo1_rd);

        int fifo2_wr = open(fifo2, O_WRONLY);
        if (fifo2_wr < 0) {
            perror("open fifo2 for writing");
            exit(1);
        }

        char output[BUFFER_SIZE];
        snprintf(output, BUFFER_SIZE, "Количество \"слов\" в файле: %d\n", count);
        write(fifo2_wr, output, strlen(output));
        close(fifo2_wr);
        exit(0);
    } 
    else { 
        int fd_in = open(argv[1], O_RDONLY);
        if (fd_in < 0) {
            perror("open input file");
            wait(NULL); 
            clean();
            exit(1);
        }

        int fifo1_wr = open(fifo1, O_WRONLY);
        if (fifo1_wr < 0) {
            perror("open fifo1 for writing");
            close(fd_in);
            wait(NULL); 
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
                wait(NULL);
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
            wait(NULL);
            clean();
            exit(1);
        }

        char result[BUFFER_SIZE];
        int bytes_result = read(fifo2_rd, result, BUFFER_SIZE);
        
        close(fifo2_rd);

        if (bytes_result <= 0) {
            perror("read from fifo2");
            wait(NULL);
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
        wait(NULL);
        
        clean();
    }

    return 0;
}