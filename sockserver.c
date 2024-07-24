#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>

#define MAX_MSG_LEN 1024

//function to write on the files
void RegToLog(FILE* fname, const char * message){
    time_t act_time;
    time(&act_time);
    int lock_file = flock(fileno(fname), LOCK_EX);
    if(lock_file == -1){
        perror("failed to lock the file");
        return;
    }
    fprintf(fname, "%s : ", ctime(&act_time));
    fprintf(fname, "%s\n", message);
    fflush(fname);

    int unlock_file = flock(fileno(fname), LOCK_UN);
    if(unlock_file == -1){
        perror("failed to unlock the file");
    }
}

void Recieve(int sockfd, char *buffer, int* pipetowrite, FILE *debug){
    //function retrieve the message from the client and echo back it writes to the parent the message as an input
    FILE* error = fopen("files/error.log", "a");
    if(recv(sockfd, buffer, MAX_MSG_LEN, 0) < 0){
        RegToLog(error, "TARGET: SOCKETSERVER error recieving message frome client");
        exit(EXIT_FAILURE);
    }
    RegToLog(debug, buffer);
    if (write(*pipetowrite, buffer, strlen(buffer)+1) < 0){
        RegToLog(error, "SOCKSERVER: error in writing the message");
        exit(EXIT_FAILURE);
    }
    if(send(sockfd, buffer, strlen(buffer)+1, 0) < 0){
        RegToLog(error, " SOCKETSERVER: error in sending message to client");
        exit(EXIT_FAILURE);
    }
    fclose(error);
}

void Send(int sock, char *msg, FILE *debug){
    //function to send a message and recieve an echo
    FILE* error = fopen("files/error.log", "a");
    if(send(sock, msg, strlen(msg)+1, 0) == -1){
        perror("send");
        RegToLog(error, "SOCKSERVER: error in sending message to server");
        exit(EXIT_FAILURE);
    }
    RegToLog(debug, msg);
    char recvmsg[MAX_MSG_LEN];
    if(recv(sock, recvmsg, MAX_MSG_LEN, 0) < 0){
        perror("recv");
        RegToLog(error, "SOCKSERVER: error in recieving message frome the server");
        exit(EXIT_FAILURE);
    }
    RegToLog(debug, "Message echo:");
    RegToLog(debug, recvmsg);
    if(strcmp(recvmsg, msg) != 0){
        RegToLog(error, "SOCKSERVER: echo not right");
        exit(EXIT_FAILURE);
    }
    fclose(error);
}

int main(int argc, char* argv[]){

    FILE* error = fopen("files/error.log", "a");
    FILE* routine = fopen("files/routine.log", "a");

    RegToLog(routine, "SOCKSERVER: start");

    char msg[MAX_MSG_LEN];

    int pipesock[2];
    int sockfd;
    int identifier;  //to identify the child process (0=target, 1=obstacle)
    char stop[] = "STOP";
    char ge[] = "GE";
    bool stopcheck = false;

    sscanf(argv[4], "%d", &identifier);  //identifies the child
    sprintf(msg, "files/sock%d.log", identifier);

    FILE* socklog = fopen(msg, "w");
    RegToLog(socklog, "server socket up");

    sscanf(argv[1], "%d", &sockfd);
    sscanf(argv[2], "%d", &pipesock[0]);
    sscanf(argv[3], "%d", &pipesock[1]);
    RegToLog(socklog, "pipes creted");

    fd_set readfd;
    FD_ZERO(&readfd);
    memset(msg, '\0', MAX_MSG_LEN);
    Recieve(sockfd, msg, &pipesock[1], socklog);

    Send(sockfd, argv[5], socklog);

    while(!stopcheck){

        FD_SET(pipesock[0], &readfd);
        FD_SET(sockfd, &readfd);

        int maxfd;
        if(pipesock[0] > sockfd)
            maxfd = pipesock[0];
        else
            maxfd = sockfd;
        int sel = select(maxfd+1, &readfd, NULL, NULL, NULL);
        if(sel < 0){
            RegToLog(error, "SOCKSERVER: error in select");
            perror("select");
            exit(EXIT_FAILURE);
        }
        else if(sel > 0){
            if(FD_ISSET(sockfd, &readfd)){
                memset(msg, '\0', MAX_MSG_LEN);
                Recieve(sockfd, msg, &pipesock[1], socklog);
            }
            if(FD_ISSET(pipesock[0], &readfd)){
                memset(msg, '\0', MAX_MSG_LEN);
                if(read(pipesock[0], msg, MAX_MSG_LEN) < 0){
                    RegToLog(error, "SOCKSERVER: error in reading from pipe");
                    perror("reading from pipe");
                    exit(EXIT_FAILURE);
                }
                RegToLog(socklog, msg);
                Send(sockfd, msg, socklog);
                RegToLog(socklog, "message sent to client");
            }
        }
        else{
            RegToLog(socklog, "timeout expired");
        }
        if(strcmp(msg, stop) == 0)
            stopcheck = true;
    }

    RegToLog(routine, "SOCKSERVER: closing return value 0");

    close(sockfd);
    close(pipesock[0]);
    close(pipesock[1]);
    fclose(error);
    fclose(routine);
    fclose(socklog);

    return 0;

}