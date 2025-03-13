
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 128
#define REQUEST 1
#define RESPONSE 2

struct message {
    long mtype;              
    char text[BUFFER_SIZE];    
};


int main(int argc, char *argv[]) {
    
    int opt;
    char *input_file = NULL;
    char *output_file = NULL;

    if (argc != 3) {
        char *usage = "Usage: ./program <input_file> <output_file>\n";
        write(1, usage, strlen(usage));
        exit(1);
    }
    input_file = argv[1];
    output_file = argv[2];

    int fd_in = open(input_file, O_RDONLY);
    if (fd_in < 0) {
        perror("open input_file");
        exit(1);
    }

    int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        perror("open output_file");
        close(fd_in);
        exit(1);
    }

    key_t key = 0x52;
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid < 0) {
        perror("msgget");
        close(fd_in);
        close(fd_out);
        exit(1);
    }
    
    struct message msg;
    int bytes_read;

    while ((bytes_read = read(fd_in, msg.text, BUFFER_SIZE)) > 0) {        
        msg.mtype = REQUEST;
        if (msgsnd(msgid, &msg, bytes_read, 0) < 0) {
            perror("msgsnd");
            close(fd_in);
            close(fd_out);
            exit(1);
        }
    }
    if (bytes_read < 0) {
        perror("read");
        close(fd_in);
        close(fd_out);
        exit(1);
    }

    msg.mtype = REQUEST;
    memset(msg.text, 0, BUFFER_SIZE);
    strncpy(msg.text, "EOF", BUFFER_SIZE );
    if (msgsnd(msgid, &msg, strlen(msg.text) , 0) < 0) {
        perror("msgsnd EOF");
        close(fd_in);
        close(fd_out);
        exit(1);
    }
    close(fd_in);

    while (1) {
        struct message response;
        int bytes_resp = msgrcv(msgid, &response, BUFFER_SIZE, RESPONSE, 0);

        if (bytes_resp < 0) {
            perror("msgrcv");
            close(fd_out);
            exit(1);
        }

        if (strncmp(response.text, "EOF", 3) == 0) {
            break;
        }
        if (write(fd_out, response.text, bytes_resp) < 0) {
            perror("write");
            close(fd_out);
            exit(1);
        }
    }
    close(fd_out);

    if (msgctl(msgid, IPC_RMID, NULL) < 0) {
        perror("msgctl");
        exit(EXIT_FAILURE);
    }

    return 0;
}






