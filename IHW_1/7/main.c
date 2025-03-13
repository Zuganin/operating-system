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
        while ((bytes_read = read(fifo1_rd, buffer, BUFFER_SIZE)) > 0) {
            processText(buffer, bytes_read, prevPart, &weCanMakeNewWord, &firstLetterInWordIsAlpha);
        }
    
        close(fifo1_rd);

        int fifo2_wr = open(fifo2, O_WRONLY);
        if (fifo2_wr < 0) {
            perror("open fifo2 for writing");
            exit(1);
        }

        char output[BUFFER_SIZE];
        snprintf(output, BUFFER_SIZE, "Количество уникальных \"слов\" в файле: %d\n", wordCount);
        write(fifo2_wr, output, strlen(output));
        close(fifo2_wr);
        freeWords();
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