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
#include <pthread.h>

#define MAX_MSG_LEN 1024

pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;

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
    fd_set read_fd, write_fd;
    bool ok = false;
    struct timeval timeout3;
    timeout3.tv_sec = 4;
    timeout3.tv_usec = 0;
    ssize_t prova_peer;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout3, sizeof(timeout3));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout3, sizeof(timeout3));
    //pthread_mutex_lock(&socket_mutex);
    while(!ok){
        FD_ZERO(&read_fd);
        FD_SET(sockfd, &read_fd);
        int sel2;
        do{
            sel2 = select(sockfd+1, &read_fd, NULL, NULL, &timeout3);
        }while(sel2<0 && errno == EINTR);
        if(sel2 < 0){
            RegToLog(debug, "sockeserver: error in select nel recieve");
            perror("select2");
            fclose(error);
            //pthread_mutex_unlock(&socket_mutex);
            exit(EXIT_FAILURE);
        }
        else if(sel2>0){
            pthread_mutex_lock(&socket_mutex);
            if(prova_peer = recv(sockfd, buffer, MAX_MSG_LEN, 0) < 0){
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    RegToLog(debug, "not recv in the time in the recv in the Recieve");
                    pthread_mutex_unlock(&socket_mutex);
                    continue;
                }
                RegToLog(debug, "TARGET: SOCKETSERVER error recieving message frome client");
                fclose(error);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            //pthread_mutex_unlock(&mutex);
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta recv della Recieve");
            pthread_mutex_unlock(&socket_mutex);
            ok = true;
        }
        /*else
            RegToLog(debug, "select1 in the recieve function = 0");*/
    }
    ok = false;  
    RegToLog(debug, buffer);
    RegToLog(debug, "in the recieve function");
    
    if (write(*pipetowrite, buffer, strlen(buffer)+1) < 0){
        RegToLog(error, "SOCKSERVER: error in writing the message");
        exit(EXIT_FAILURE);
    }
    fsync(*pipetowrite);
    sleep(1);
    
    while(!ok){
        FD_ZERO(&write_fd);
        FD_SET(sockfd, &write_fd);
        int sel;
        do{
            sel = select(sockfd+1, NULL, &write_fd, NULL, &timeout3);
        }while(sel<0 && errno == EINTR);
        if(sel < 0){
            RegToLog(debug, "sockeserver: error in select nel recieve 2");
            perror("select");
            fclose(error);
            //pthread_mutex_unlock(&socket_mutex);
            exit(EXIT_FAILURE);
        }
        else if(sel >0){
            pthread_mutex_lock(&socket_mutex);
            if(prova_peer = send(sockfd, buffer, strlen(buffer)+1, 0) < 0){
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    RegToLog(debug, "not send in the time in the send in the Recueve");
                    pthread_mutex_unlock(&socket_mutex);
                    continue;
                }
                RegToLog(debug, " SOCKETSERVER: error in sending message to client");
                fclose(error);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta send della Recieve");
            pthread_mutex_unlock(&socket_mutex);
            ok = true;
        }
        /*else
            RegToLog(debug, "select2 in the recieve function = 0");*/
    }
    //pthread_mutex_unlock(&socket_mutex);
    fclose(error);
}

void Send(int sock, char *msg, FILE *debug){
    //function to send a message and recieve an echo
    FILE* error = fopen("files/error.log", "a");
    RegToLog(error, msg);
    fd_set read_fd, write_fd;
    bool ok = false;
    struct timeval timeout3;
    timeout3.tv_sec = 4;
    timeout3.tv_usec = 0;
    ssize_t prova_peer;
    char recvmsg[MAX_MSG_LEN];
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout3, sizeof(timeout3));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout3, sizeof(timeout3));
    //pthread_mutex_lock(&socket_mutex);
    while(!ok){
        
        FD_ZERO(&write_fd);
        FD_SET(sock, &write_fd);
        int sel;
        do{
            sel = select(sock+1, NULL, &write_fd, NULL, &timeout3);
        }while(sel<0 && errno == EINTR);
        if(sel < 0){
            RegToLog(debug, "sockeserver: error in select nel send 1");
            perror("select");
            fclose(error);
            //pthread_mutex_unlock(&socket_mutex);
            exit(EXIT_FAILURE);
        }
        else if(sel >0){
            pthread_mutex_lock(&socket_mutex);
            if(prova_peer = send(sock, msg, strlen(msg)+1, 0) == -1){
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    RegToLog(debug, "not send in the time in the send in the Send");
                    pthread_mutex_unlock(&socket_mutex);
                    continue;
                }
                perror("send");
                RegToLog(debug, "SOCKSERVER: error in sending message to server");
                fclose(error);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta send della Send");
            pthread_mutex_unlock(&socket_mutex);
            ok = true;
        }
        /*else
            RegToLog(debug, "select in the send function = 0");*/
    }
    ok = false;
    RegToLog(debug, msg);
    RegToLog(debug, "in the send function");
    
    while(!ok){
        
        FD_ZERO(&read_fd);
        FD_SET(sock, &read_fd);
        int sel2;
        do{
            sel2 = select(sock+1, &read_fd, NULL, NULL, &timeout3);
        }while(sel2<0 && errno == EINTR);
        if(sel2 <0){
            RegToLog(debug, "sockeserver: error in select nel send");
            perror("select2");
            fclose(error);
            //pthread_mutex_unlock(&socket_mutex);
            exit(EXIT_FAILURE);
        }
        if(sel2 > 0){
            pthread_mutex_lock(&socket_mutex);
            if(prova_peer = recv(sock, recvmsg, MAX_MSG_LEN, 0) < 0){
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    RegToLog(debug, "not recv in the time in the recv in the Send");
                    pthread_mutex_unlock(&socket_mutex);
                    continue;
                }
                perror("recv");
                RegToLog(debug, "SOCKSERVER: error in recieving message frome the server");
                fclose(error);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta recv della Send");
            pthread_mutex_unlock(&socket_mutex);
            ok = true;
        }
        /*else 
            RegToLog(debug, "select in the send function serversock =0");*/
    }
    RegToLog(debug, "Message echo:");
    RegToLog(debug, recvmsg);
    if(strcmp(recvmsg, msg) != 0){
        RegToLog(debug, "SOCKSERVER: echo not right");
        exit(EXIT_FAILURE);
    }
    //pthread_mutex_unlock(&socket_mutex);
    fclose(error);
}

int main(int argc, char* argv[]){

    FILE* error = fopen("files/error.log", "a");
    FILE* routine = fopen("files/routine.log", "a");
    //pthread_mutex_init(&mutex, NULL);
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
    
    memset(msg, '\0', MAX_MSG_LEN);
    Recieve(sockfd, msg, &pipesock[1], socklog);

    Send(sockfd, argv[5], socklog);

    int maxfd = pipesock[0] > sockfd ? pipesock[0] : sockfd;
    struct timeval timeout;
    timeout.tv_sec = 4;
    timeout.tv_usec = 0;
    //sleep(1);
    while(!stopcheck){
        
        FD_ZERO(&readfd);
        FD_SET(sockfd, &readfd);
        FD_SET(pipesock[0], &readfd);
        
        RegToLog(socklog, "entering in the loop");
        int sel;
        do{
            sel = select(maxfd+1, &readfd, NULL, NULL, &timeout);
            sprintf(msg, "sel %d", sel);
            RegToLog(socklog, msg);
            sleep(1);
        }while (sel == -1 && errno == EINTR );
        if(sel < 0){
            RegToLog(error, "SOCKSERVER: error in select");
            perror("select");
            fclose(error);
            fclose(routine);
            fclose(socklog);
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
                    fclose(error);
                    fclose(routine);
                    fclose(socklog);
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
    pthread_mutex_destroy(&socket_mutex);
    close(sockfd);
    close(pipesock[0]);
    close(pipesock[1]);
    fclose(error);
    fclose(routine);
    fclose(socklog);

    return 0;

}