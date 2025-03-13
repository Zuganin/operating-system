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

        words = malloc(sizeof(char*) * 1024);
        if (!words) {
            perror("malloc");
            exit(1);
        }

        char buffer[BUFFER_SIZE];
        int bytes_read;
        char prevPart[BUFFER_SIZE];
        prevPart[0] = '\0';
        int weCanMakeNewWord = 1;
        int firstLetterInWordIsAlpha = 0;

        while((bytes_read = read(fifo1_in, buffer, BUFFER_SIZE))> 0) {
            if (bytes_read < 0) {
                perror("read from fifo1");
                exit(1);
            }
            processText(buffer,bytes_read, prevPart, &weCanMakeNewWord, &firstLetterInWordIsAlpha);
        }
        
        close(fifo1_in);
        
        char output[BUFFER_SIZE];
        snprintf(output, BUFFER_SIZE, "Количество \"слов\" в файле: %d\n", wordCount);
        
        
        if (write(fifo2_out, output, strlen(output)) != (ssize_t)strlen(output)) {
            perror("write to fifo2");
            exit(1);
        }
        close(fifo2_out);
        freeWords();
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
