#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>


#define BUFFER_SIZE 5000



char **words;
int wordCount = 0;

int uniqueWord(char *newWord) {
    int isUnique = 1;
    for (int i = 0; i < wordCount; i++) {
        if (strcmp(words[i], newWord) == 0) {
            isUnique = 0;  
            break;
        }
    }
    return isUnique;
}

void freeWords() {
    for (int i = 0; i < wordCount; i++) {
        free(words[i]);
    }
    free(words);
}


int processText(char *buffer, int buf_size, char *prevPart, int *weCanMakeNewWord, int *firstLetterInWordIsAlpha) {
    int uniqueCount = 0;
    char word[BUFFER_SIZE];
    int wordIndex = 0;

    if (*weCanMakeNewWord) {
        strcpy(word, prevPart);
        wordIndex = strlen(prevPart);
    } else {
        word[0] = '\0';
    }

    for (int i = 0; i < buf_size; i++) {
        char ch = buffer[i];

        if (isdigit(ch) && !*firstLetterInWordIsAlpha) {
            *weCanMakeNewWord = 0;
            *firstLetterInWordIsAlpha = 0;
            continue;
        }

        if (isalpha(ch) && *weCanMakeNewWord) {
            if (wordIndex < BUFFER_SIZE -1 ) {  
                word[wordIndex++] = ch;
            }
            *firstLetterInWordIsAlpha = 1;
            continue;
        }

        if (isalnum(ch) && *firstLetterInWordIsAlpha) {
            if (wordIndex < BUFFER_SIZE -1 ) {
                word[wordIndex++] = ch;
            }
            continue;
        }

        if (!isalnum(ch) && *firstLetterInWordIsAlpha) {
            word[wordIndex] = '\0'; 
            if (uniqueWord(word)) {
                words[wordCount] = strdup(word);
                uniqueCount++;
                wordCount++;
            }
            word[0] = '\0';
            wordIndex = 0;
            *weCanMakeNewWord = 0;
            *firstLetterInWordIsAlpha = 0;
            continue;
        }

        if (!isalnum(ch) && !*weCanMakeNewWord) {
            *weCanMakeNewWord = 1;
            continue;
        }
    }
    if (buf_size < BUFFER_SIZE) {
        if (isalnum(buffer[buf_size-1]) && *firstLetterInWordIsAlpha) {
            word[wordIndex] = '\0'; 
            if (uniqueWord(word)) {
                words[wordCount] = strdup(word);
                uniqueCount++;
                wordCount++;
            }
            word[0] = '\0';
            wordIndex = 0;
            *weCanMakeNewWord = 0;
            *firstLetterInWordIsAlpha = 0;
        }
    }

    word[wordIndex] = '\0';
    strcpy(prevPart, word);
    return uniqueCount;
}

int main(int argc, char *argv[]) {
    
    if (argc != 3) {
        char *usage = "Usage: ./program <input_file> <output_file>\n";
        write(STDOUT_FILENO, usage, strlen(usage));
        exit(1);
    }

    words = malloc(sizeof(char*) * 1024);
    if (!words) {
        perror("malloc");
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
    
    // 1 Дочерний процесс - читает данные из файла
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
    
    // 2 Дочерний процесс - считает количество "слов"
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
        char prevPart[BUFFER_SIZE];
        prevPart[0] = '\0';
        int weCanMakeNewWord = 1;
        int firstLetterInWordIsAlpha = 0;
        for(int i = 0; (bytes_read = read(pipe1[0], buffer, BUFFER_SIZE))> 0;++i) {
            if (bytes_read < 0) {
                perror("read from pipe1");
                exit(1);
            }
            processText(buffer,bytes_read, prevPart, &weCanMakeNewWord, &firstLetterInWordIsAlpha);
        }
        close(pipe1[0]);
        
        char output[BUFFER_SIZE];
        snprintf(output, BUFFER_SIZE, "Количество уникальных \"слов\" в файле: %d\n", wordCount);
        

        
        if (write(pipe2[1], output, strlen(output)) != (ssize_t)strlen(output)) {
            perror("write to pipe2");
            exit(1);
        }
        freeWords();
        close(pipe2[1]);
        exit(0);
    }
    
    
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[1]); 
    
    // 3 Родительский процесс - записывает в файл.
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
