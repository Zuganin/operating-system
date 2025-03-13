#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 128
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


int main(){
    words = malloc(sizeof(char*) * 1024);
    if (!words) {
        perror("malloc");
        exit(1);
    }
    int fifo1_rd = open(fifo1, O_RDONLY);
    if (fifo1_rd < 0) {
        perror("open fifo1 for reading");
        exit(1);
    }
    int bytes_read;
    char buffer[BUFFER_SIZE];
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
    exit(0);

};




