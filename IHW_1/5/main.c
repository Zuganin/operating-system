#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

    if (mkfifo(fifo1, 0666) < 0 || mkfifo(fifo2, 0666) < 0) {
        perror("mkfifo error");
        clean();
        exit(1);
    }
    
    // 1 Дочерний процесс - считывает файл.
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork");
        exit(1);
    }
    
    if (pid1 == 0) {
        int fd_in = open(argv[1], O_RDONLY);
        if (fd_in < 0) {
            perror("open input file");
            exit(1);
        }
        
        int fifo1_out = open(fifo1, O_WRONLY);
        if(fifo1_out < 0){
            perror("fifo1_out");
            exit(1);
        }
        char buffer[BUFFER_SIZE];

        int bytes_read;
        while((bytes_read = read(fd_in, buffer, BUFFER_SIZE)) > 0){
            if (bytes_read < 0) {
                perror("read input file");
                exit(1);
            }
            
            if (write(fifo1_out, buffer, bytes_read) != bytes_read) {
                perror("write to fifo1_out");
                exit(1);
            }
        } 
        
        close(fd_in);
        close(fifo1_out); 
        exit(0);
    }
    
    // 2 Дочерний процесс – считывает данные из fifo1_out и считает количество слов
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        exit(1);
    }
    
    if (pid2 == 0) {
        int fifo1_in = open(fifo1, O_RDONLY);
        int fifo2_out = open(fifo2, O_WRONLY);
        char buffer[BUFFER_SIZE];
        int bytes_read;
        int count = 0;
        int in_word = 0;
        int word_started_with_letter = 0;
        
        while((bytes_read = read(fifo1_in, buffer, BUFFER_SIZE))> 0) {
            if (bytes_read < 0) {
                perror("read from fifo1");
                exit(1);
            }
            count += process_text(buffer,bytes_read, &in_word, &word_started_with_letter);
        }
        
        if (in_word && word_started_with_letter) {
            count++;
        }

        close(fifo1_in);
        
        char output[BUFFER_SIZE];
        snprintf(output, BUFFER_SIZE, "Количество \"слов\" в файле: %d\n", count);
        
        
        if (write(fifo2_out, output, strlen(output)) != (ssize_t)strlen(output)) {
            perror("write to fifo2");
            exit(1);
        }
        close(fifo2_out);
        exit(0);
    }
    
    // 3 Родительский процесс – считывает данные с fifo2_out и записывает в файл
    int fifo2_in = open(fifo2, O_RDONLY);
    char result[BUFFER_SIZE];
    int bytes_result = read(fifo2_in, result, BUFFER_SIZE);
    if (bytes_result < 0) {
        perror("read from fifo2");
        exit(1);
    }
    close(fifo2_in);
    
    
    int fd_out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        perror("open output file");
        exit(1);
    }
    if (write(fd_out, result, bytes_result) != bytes_result) {
        perror("write to output file");
        exit(1);
    }
    close(fd_out);
   
    wait(NULL);
    wait(NULL);
    
    clean();

    return 0;
}
