#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <termios.h>
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
#include <math.h>
#include <stdbool.h>
#include <time.h>

#define MAX_LINE_LENGHT 256
#define MAX_MSG_LEN 1024

typedef struct{
    float x;
    float y;
    bool taken;
} targets;

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

void Recieve(int sockfd, char *buffer, FILE *debug){
    //function retrieve the message from the client and echo back
    FILE* error = fopen("files/error.log", "a");
    if(recv(sockfd, buffer, MAX_MSG_LEN, 0) < 0){
        RegToLog(error, "TARGET: SOCKETSERVER error recieving message frome client");
        exit(EXIT_FAILURE);
    }
    RegToLog(debug, buffer);
    if(send(sockfd, buffer, strlen(buffer)+1, 0) < 0){
        RegToLog(error, "TARGET: SOCKETSERVER error in sending message to client");
        exit(EXIT_FAILURE);
    }
    fclose(error);
}

void Send(int sock, char *msg, FILE *debug){
    //function to send a message and recieve an echo
    FILE* error = fopen("files/error.log", "a");
    if(send(sock, msg, strlen(msg)+1, 0) == -1){
        perror("send");
        RegToLog(error, "TARGET: error in sending message to server");
        exit(EXIT_FAILURE);
    }
    char recvmsg[MAX_MSG_LEN];
    if(recv(sock, recvmsg, MAX_MSG_LEN, 0) < 0){
        perror("recv");
        RegToLog(error, "TARGET: error in recieving message frome the server");
        exit(EXIT_FAILURE);
    }
    RegToLog(debug, "Message echo:");
    RegToLog(debug, recvmsg);
    if(strcmp(recvmsg, msg) != 0){
        RegToLog(error, "TARGET: echo not right");
        exit(EXIT_FAILURE);
    }
    fclose(error);
}

int ncoord = 0;
//function to see if the created points are by chance equal
int point_feseability( int coord[2][ncoord], int maxx, int maxy){
    int coordx[ncoord];
    int coordy[ncoord];
    for(int j=0; j<ncoord; j++){
        coordx[j] = coord[0][j];
        coordy[j] = coord[1][j];
    }

    int cc =0;
    for(int i=0; i<ncoord; i++){
        for(int g =0; g<ncoord; g++){
            if(i != g){
                if(coordx[i] == coordx[g] && coordy[i] == coordy[g])
                    cc++;
            }
            
        }
    }

    if(cc != 0)
        return 1;

    return 0;

}

int main(int argc, char* argv[]){

    FILE* routine = fopen("files/routine.log", "a");
    FILE* error = fopen("files/error.log", "a");
    FILE* tarlog = fopen("files/targets.log", "a");

    if(error == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    if(routine == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    if(tarlog == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    RegToLog(routine, "TARGET : start\n");

    //fprintf(tarlog, "start acquiring from data.txt \n");
    int fp;
    const char* filename = "files/data.txt";
    char line[MAX_LINE_LENGHT];

    //reading from the data.txt file the number of targets to create
    fp = open(filename, O_RDONLY);
    if(fp == -1){
        perror("fp opening");
        RegToLog(error, "TARGET: error in opening fp");
        exit(EXIT_FAILURE);
    }
    int lock_file = flock(fp, LOCK_SH);
    if(lock_file == -1){
        perror("failed to lock the file pid");
        RegToLog(error, "TARGET; error in lock the failmsmsms");
        exit(EXIT_FAILURE);
    }
    FILE* file = fdopen(fp, "r");
    
    while(fgets(line, sizeof(line), file) != NULL){
        char label[MAX_LINE_LENGHT];
        int value;
        if(sscanf(line, "%[^:]:%d", label, &value) == 2){
            if(strcmp(label, "N_TARGET") == 0){
                ncoord = value;
                break;
            }
        }
        else{
            fprintf(tarlog, "problems in the number");
        }
    }

    int unlock_file = flock(fp, LOCK_UN);
    if(unlock_file == -1){
        perror("failed to unlock the file pid");
    }

    fclose(file);
    fprintf(tarlog, "n_tar: %d  \n", ncoord);
    fflush(tarlog);

    char msg[100];  //to write on the logfile
    targets* target[ncoord];
    struct sockaddr_in server_address;

    struct hostent *server;
    int port = 40000;  //default port
    int sock;
    char sockmsg[MAX_MSG_LEN];
    float r, c;
    int maxx, maxy;
    char stop[] = "STOP";
    char message[] = "TI";
    char ge[] = "GE";
    bool stoprec = false;
    bool gecheck = false;

    fd_set readfd;
    FD_ZERO(&readfd);

    sscanf(argv[1], "%d", &port);
    sprintf(msg, "TARGET: port = %d", port);
    RegToLog(tarlog, msg);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1){
        perror("socket");
        RegToLog(error, "TARGET: error in creating the socket");
        return 1;
    }

    RegToLog(tarlog, "sock ok");

    bzero((char*)&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    //convert string in ip address
    if(inet_pton(AF_INET, argv[2], &server_address.sin_addr) < 0){
        perror("inet_pton");
        RegToLog(error, "TARGET: error in inet_pton()");
        return 1;
    }
    RegToLog(tarlog, "inet pton ok");
    //connect to server
    if((connect(sock, (struct sockaddr*)&server_address, sizeof(server_address))) == -1){
        perror("connect");
        RegToLog(error, "TARGET: error in connecting to server");
        return 1;
    }
    RegToLog(tarlog, "connected to serverSocket");

    RegToLog(tarlog, message);
    Send(sock, message, tarlog);
    //recienve dimentions of the window
    memset(sockmsg, '\0', MAX_MSG_LEN);
    Recieve(sock, sockmsg, tarlog);
    
    char *format = "%f,%f";
    sscanf(sockmsg, format, &r, &c);
    maxx = (int)c;
    maxy = (int)r;
    fprintf(tarlog, "\n x:%d  y:%d\n", maxx, maxy);
    fflush(tarlog);
    int points[2][ncoord];
    sleep(2);

    /*for(int jjj = 0; jjj < ncoord; jjj++){
        fprintf(tarlog, "coord n %d: x(%d) y (%d)\n", jjj, points[0][jjj], points[1][jjj]);
        fflush(tarlog);
    }*/

    //sending the coordinates untill the server gives back the stop signal
    while(!stoprec){
        gecheck = false;
        char pos_targets[ncoord][10];
        char targetstr[MAX_MSG_LEN];
        char temp[50];
        srand(time(NULL));  //initialise random seed

        //add the number of targets to the string
        sprintf(targetstr, "T[%d]", ncoord);
        for(int i = 0; i < ncoord; i++){
            //generate the coordinates
            target[i]->x = rand() % maxx;
            target[i]->y = rand() % maxy;
            target[i]->taken = false;
            points[0][i] = target[i]->x;
            points[1][i] = target[i]->y;
        }

        while(point_feseability(points, maxx, maxy)){
            for(int ii = 0; ii<2; ii++){
                for(int jj=0; jj<ncoord; jj++){
                    if(ii == 0)
                        points[ii][jj] = rand() %maxx;  //random column
                    else
                        points[ii][jj] = rand() %maxy;  //random random raw
                }
            }
        }

        for(int b=0; b < ncoord; b++){
            points[0][b] = target[b]->x;
            points[1][b] = target[b]->y;
            sprintf(temp, "%.3f,%.3f|", target[b]->x, target[b]->y);
            strcat(targetstr, temp);
            sprintf(msg, "TAREGET: targets %d: x=%.3f  y=%.3f\n", b, target[b]->x, target[b]->y);
            RegToLog(tarlog, msg);
        }
        targetstr[strlen(targetstr)-1] = '\0';  //remove the last |
        RegToLog(tarlog, targetstr);

        //send the targets
        Send(sock, targetstr, tarlog);
        int sel;
        while(!gecheck){
            FD_SET(sock, &readfd);
            do{
                sel = select(sock+1, &readfd, NULL, NULL, NULL);
            }while(sel<0 && errno == EINTR);
            if(sel>0){
                if(FD_ISSET(sock, &readfd)){
                    RegToLog(tarlog, "reading the message from the socket");
                    char buffer[MAX_MSG_LEN];
                    Recieve(sock, buffer, tarlog);
                    if(strcmp(buffer, stop) == 0){
                        RegToLog(tarlog, "stop message recieved");
                        stoprec = true;
                        gecheck = true;
                    }
                    else if(strcmp(buffer, ge) == 0){
                        gecheck = true;
                        RegToLog(tarlog, "ge message recieved");
                    }
                }
            }
        }
    }
    
    RegToLog(tarlog, "exiting return value 0");

    //close the files
    close(sock);
    fclose(tarlog);
    fclose(routine);
    fclose(error);
    
    return 0;
}