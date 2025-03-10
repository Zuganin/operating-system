#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>


#define BUFFER_SIZE 5000


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
    
    int pipe1[2]; 
    int pipe2[2]; 


    if (pipe(pipe1) == -1) {
        perror("pipe1");
        exit(1);
    }
    

    if (pipe(pipe2) == -1) {
        perror("pipe2");
        exit(1);
    }
    
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork");
        exit(1);
    }
    
    if (pid1 == 0) {
    
        close(pipe1[0]); 
        close(pipe2[0]);
        close(pipe2[1]);
        
        int fd_in = open(argv[1], O_RDONLY);
        if (fd_in < 0) {
            perror("open input file");
            exit(1);
        }
        
        char buffer[BUFFER_SIZE];

        int bytes_read;
        while((bytes_read = read(fd_in, buffer, BUFFER_SIZE)) > 0){
            if (bytes_read < 0) {
                perror("read input file");
                exit(1);
            }
            
            if (write(pipe1[1], buffer, bytes_read) != bytes_read) {
                perror("write to pipe1");
                exit(1);
            }
        } 
        
        close(fd_in);
        close(pipe1[1]); 
        exit(0);
    }
    
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        exit(1);
    }
    
    if (pid2 == 0) {
        close(pipe1[1]); 
        close(pipe2[0]); 
        
        char buffer[BUFFER_SIZE];
        int bytes_read;
        int count = 0;
        int in_word = 0;
        int word_started_with_letter = 0;
        int i;
        
        for(i = 0; (bytes_read = read(pipe1[0], buffer, BUFFER_SIZE))> 0;++i) {
            if (bytes_read < 0) {
                perror("read from pipe1");
                exit(1);
            }
            count += process_text(buffer,bytes_read, &in_word, &word_started_with_letter);
        }
        
        if (in_word && word_started_with_letter) {
            count++;
        }

        close(pipe1[0]);
        
        char output[BUFFER_SIZE];
        snprintf(output, BUFFER_SIZE, "Количество \"слов\" в файле: %d\n", count);
        

        
        if (write(pipe2[1], output, strlen(output)) != (ssize_t)strlen(output)) {
            perror("write to pipe2");
            exit(1);
        }
        close(pipe2[1]);
        exit(0);
    }
    
    
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[1]); 
    
    char result[BUFFER_SIZE];
    int bytes_result = read(pipe2[0], result, BUFFER_SIZE);
    if (bytes_result < 0) {
        perror("read from pipe2");
        exit(1);
    }
    close(pipe2[0]);
    
    
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
    
    return 0;
}
