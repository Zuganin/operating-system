
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 128
#define REQUEST 1
#define RESPONSE 2

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


struct message {
    long mtype;             
    char text[BUFFER_SIZE];    
};

int main() {
    key_t key = 0x52;
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid < 0) {
        perror("msgget");
        exit(1);
    }
    words = malloc(sizeof(char*) * 1024);
    if (!words) {
        perror("malloc");
        exit(1);
    }
    
    struct message msg;

    char prevPart[BUFFER_SIZE];
    prevPart[0] = '\0';
    int weCanMakeNewWord = 1;
    int firstLetterInWordIsAlpha = 0;
    while (1) {
        int bytes_read = msgrcv(msgid, &msg, BUFFER_SIZE, REQUEST, 0);
        if (bytes_read < 0) {
            perror("msgrcv2");
            freeWords();
            exit(1);
        }
        if (strncmp(msg.text, "EOF", 3) == 0) {
            break;
        }
        processText(msg.text, bytes_read, prevPart, &weCanMakeNewWord, &firstLetterInWordIsAlpha);        
    }
    msg.mtype = RESPONSE;
    snprintf(msg.text, BUFFER_SIZE, "Количество уникальных слов: %d\n", wordCount);
    if (msgsnd(msgid, &msg, strlen(msg.text) , 0) < 0) {
        perror("msgsnd");
        exit(1);
    }

    strncpy(msg.text, "EOF", 3);
    msg.mtype = RESPONSE;
    if (msgsnd(msgid, &msg, strlen(msg.text) , 0) < 0) {
        perror("msgsnd EOF");
        exit(EXIT_FAILURE);
    }
    freeWords();
    
    return 0;
}
