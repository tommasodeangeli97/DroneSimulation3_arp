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
#include <pthread.h>

#define MAX_LINE_LENGHT 256
#define MAX_MSG_LEN 1024
//pthread_mutex_t mutex;
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

void Recieve(int sockfd, char *buffer, FILE *debug){
    //function retrieve the message from the client and echo back
    FILE* sor_log = fopen("files/select_obst_recieve.log", "a");
    RegToLog(sor_log, "OBST: recieve start");

    fd_set read_fd, write_fd;
    bool ok = false;
    struct timeval timeout3;
    timeout3.tv_sec = 4;
    timeout3.tv_usec = 0;
    ssize_t prova_peer;
    //pthread_mutex_lock(&socket_mutex);
    while(!ok){
        FD_ZERO(&read_fd);
        FD_SET(sockfd, &read_fd);
        int sel2;
        do{
            sel2 = select(sockfd+1, &read_fd, NULL, NULL, &timeout3);
        }while(sel2<0 && errno == EINTR);
        if(sel2 < 0){
            RegToLog(sor_log, "obst :: error in select nel recieve");
            perror("select2");
            fclose(sor_log);
            //pthread_mutex_unlock(&socket_mutex);
            exit(EXIT_FAILURE);
        }
        else if(sel2 > 0){
            pthread_mutex_lock(&socket_mutex);
            if(recv(sockfd, buffer, MAX_MSG_LEN, 0) < 0){
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    RegToLog(debug, "not recv in the time in the recv in the recieve");
                    pthread_mutex_unlock(&socket_mutex);
                    continue;
                }
                RegToLog(sor_log, "obstacles: SOCKETSERVER error recieving message frome client");
                fclose(sor_log);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta recv della Recieve");
            pthread_mutex_unlock(&socket_mutex);
            ok = true;
        }
        /*else
            RegToLog(sor_log, "obst :: select1 in the recieve function = 0");*/
    }
    ok = false;
    RegToLog(debug, buffer);
    RegToLog(debug, "OBST: recv ok");
    while(!ok){
        FD_ZERO(&write_fd);
        FD_SET(sockfd, &write_fd);
        int sel;
        do{
            sel = select(sockfd+1, NULL, &write_fd, NULL, &timeout3);
        }while(sel<0 && errno == EINTR);
        if(sel < 0){
            RegToLog(sor_log, "sockeserver: error in select nel recieve 2");
            perror("select");
            fclose(sor_log);
            //pthread_mutex_unlock(&socket_mutex);
            exit(EXIT_FAILURE);
        }
        else if(sel >0){
            pthread_mutex_lock(&socket_mutex);
            if(send(sockfd, buffer, strlen(buffer)+1, 0) < 0){
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    RegToLog(debug, "not send in the time in the send in the recieve");
                    pthread_mutex_unlock(&socket_mutex);
                    continue;
                }
                RegToLog(sor_log, "obstacles: SOCKETSERVER error in sending message to client");
                fclose(sor_log);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta send della Recieve");
            pthread_mutex_unlock(&socket_mutex);
            ok = true;
        }
        /*else
            RegToLog(sor_log, "obst :: select2 in the recieve function = 0");*/
    }
    RegToLog(debug, "OBST: send ok");
    //pthread_mutex_unlock(&socket_mutex);
    fclose(sor_log);
}

void Send(int sock, char *msg, FILE *debug){
    //function to send a message and recieve an echo
    FILE* sos_log = fopen("files/select_obst_send.log", "a");

    fd_set read_fd, write_fd;
    bool ok = false;
    struct timeval timeout3;
    timeout3.tv_sec = 4;
    timeout3.tv_usec = 0;
    ssize_t prova_peer;
    char recvmsg[MAX_MSG_LEN];
    //pthread_mutex_lock(&socket_mutex);
    while(!ok){
        
        FD_ZERO(&write_fd);
        FD_SET(sock, &write_fd);
        int sel;
        do{
            sel = select(sock+1, NULL, &write_fd, NULL, &timeout3);
        }while(sel<0 && errno == EINTR);
        if(sel < 0){
            RegToLog(sos_log, "obst: error in select nel send 1");
            perror("select");
            fclose(sos_log);
            //pthread_mutex_unlock(&socket_mutex);
            exit(EXIT_FAILURE);
        }
        else if(sel >0){
            pthread_mutex_lock(&socket_mutex);
            if(send(sock, msg, strlen(msg)+1, 0) == -1){
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    RegToLog(debug, "not send in the time in the send in the Send");
                    pthread_mutex_unlock(&socket_mutex);
                    continue;
                }
                perror("send");
                RegToLog(sos_log, "obstacles: error in sending message to server");
                fclose(sos_log);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta send della Send");
            pthread_mutex_unlock(&socket_mutex);
            ok = true;
        }
        /*else
            RegToLog(sos_log, "select in the send function = 0");*/
    }
    ok = false;
    //sleep(1);
    RegToLog(debug, "obst :: in the send function after send");
    
    while(!ok){
        
        FD_ZERO(&read_fd);
        FD_SET(sock, &read_fd);
        int sel2;
        do{
            sel2 = select(sock+1, &read_fd, NULL, NULL, &timeout3);
        }while(sel2<0 && errno == EINTR);
        if(sel2 <0){
            RegToLog(sos_log, "obst: error in select nel send");
            perror("select2");
            fclose(sos_log);
            //pthread_mutex_unlock(&socket_mutex);
            exit(EXIT_FAILURE);
        }
        if(sel2 > 0){
            pthread_mutex_lock(&socket_mutex);
            if(recv(sock, recvmsg, MAX_MSG_LEN, 0) < 0){
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    RegToLog(debug, "not recv in the time in the recv in the Send");
                    pthread_mutex_unlock(&socket_mutex);
                    continue;
                }
                perror("recv");
                RegToLog(sos_log, "obstacles: error in recieving message frome the server");
                fclose(sos_log);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta recv della Send");
            pthread_mutex_unlock(&socket_mutex);
            ok = true;
        }
        /*else 
            RegToLog(sos_log, "obst :: select in the send function serversock =0");*/
    }
    RegToLog(debug, "Message echo:");
    RegToLog(debug, recvmsg);
    if(strcmp(recvmsg, msg) != 0){
        RegToLog(sos_log, "obstacles: echo not right");
        fclose(sos_log);
        exit(EXIT_FAILURE);
    }
    //pthread_mutex_unlock(&socket_mutex);
    fclose(sos_log);
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
    FILE* obstlog = fopen("files/obstacles.log", "a");

    if(error == NULL || routine == NULL || obstlog == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    
    //pthread_mutex_init(&mutex, NULL);
    RegToLog(routine, "OBSTACLES : start\n");

    //fprintf(obstlog, "start acquiring from data.txt \n");
    //fflush(obstlog);

    int fp;
    const char* filename = "files/data.txt";
    char line[MAX_LINE_LENGHT];

    //reading from the data.txt file the number of obstacles to create
    fp = open(filename, O_RDONLY);
    if(fp == -1){
        perror("fp opening");
        RegToLog(error, "OBSTACLES: error in opening fp");
        fclose(obstlog);
        fclose(routine);
        fclose(error);
        exit(EXIT_FAILURE);
    }
    int lock_file = flock(fp, LOCK_SH);
    if(lock_file == -1){
        perror("failed to lock the file pid");
        RegToLog(error, "OBSTACLES; error in lock the failmsmsms");
        fclose(obstlog);
        fclose(routine);
        fclose(error);
        exit(EXIT_FAILURE);
    }
    FILE* file = fdopen(fp, "r");
    
    while(fgets(line, sizeof(line), file) != NULL){
        char label[MAX_LINE_LENGHT];
        int value;
        if(sscanf(line, "%[^:]:%d", label, &value) == 2){
            if(strcmp(label, "N_OBSTACLES") == 0){
                ncoord = value;
                break;
            }
        }
        else{
            fprintf(obstlog, "problems in the pid acquisation");
        }
    }

    int unlock_file = flock(fp, LOCK_UN);
    if(unlock_file == -1){
        perror("failed to unlock the file pid");
    }

    fclose(file);
    close(fp);
    fprintf(obstlog, "n_obbst: %d  \n", ncoord);
    fflush(obstlog);

    char msg[100];  //to write on the logfile
    struct sockaddr_in server_address;

    struct hostent *server;
    int port = 3500;  //default port
    int sock;
    char sockmsg[MAX_MSG_LEN];
    float r, c;
    int maxx, maxy;
    char stop[] = "STOP";
    char message[] = "OI";
    
    bool stoprec = false;

    fd_set readfd;
    //FD_ZERO(&readfd);

    sscanf(argv[1], "%d", &port);
    sprintf(msg, "OBSTACLES: port = %d", port);
    RegToLog(obstlog, msg);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1){
        perror("socket");
        RegToLog(error, "OBSTACLES: error in creating the socket");
        return 1;
    }
    sprintf(msg, "obstacles: sock value = %d", sock);
    RegToLog(obstlog, msg);


    bzero((char*)&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    //server_address.sin_addr.s_addr = inet_addr("10.42.0.1");

    sprintf(msg, "argv[2] = %s", argv[2]);
    RegToLog(obstlog, msg);

    //convert string in ip address
    if((inet_pton(AF_INET, argv[2], &server_address.sin_addr)) <= 0){
        perror("inet_pton");
        RegToLog(error, "OBSTACLES: error in inet_pton()");
        return 1;
    }
    sprintf(msg, "obstacles: ip address = %d", server_address.sin_addr.s_addr);
    RegToLog(obstlog, msg);

    //connect to server
    if((connect(sock, (struct sockaddr*)&server_address, sizeof(server_address))) < 0){
        perror("connect");
        RegToLog(obstlog, "OBSTACLES: error in connecting to server");
        RegToLog(error, "OBSTACLES: error in connecting to server");
        return 1;
    }
    struct timeval timeout2;
    timeout2.tv_sec = 4;
    timeout2.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout2, sizeof(timeout2));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout2, sizeof(timeout2));
    RegToLog(obstlog, "connected to serverSocket");

    memset(sockmsg, '\0', MAX_MSG_LEN);

    Send(sock, message, obstlog);
    //recieve maxx and maxy
    RegToLog(obstlog, "send function finished");
    
    Recieve(sock, sockmsg, obstlog);
    RegToLog(obstlog, "recieve function finished");
    char *format = "%f,%f";
    sscanf(sockmsg, format, &r, &c);
    maxx = (int)r;
    maxy = (int)c;
    fprintf(obstlog, "\n x:%d  y:%d\n", maxx, maxy);
    fflush(obstlog);
                

    int points[2][ncoord];
    sleep(1);
    int sel;
    srand(time(NULL));  //initialise random seed
    while(!stoprec){
        
        RegToLog(obstlog, "entering in the loop");
        
        char obststr[MAX_MSG_LEN];
        char temp[50];
        

        //add the number of obstacles to the string
        sprintf(obststr, "O[%d]", ncoord);
        if(ncoord == 0)
            strcat(obststr, "|");
        for(int i = 0; i < ncoord; i++){
            //generate the coordinates
            points[0][i] = (rand() % (maxx-1))+1;
            points[1][i] = (rand() % (maxy-1))+1;
        }

        while(point_feseability(points, maxx, maxy)){
            for(int ii = 0; ii<2; ii++){
                for(int jj=0; jj<ncoord; jj++){
                    if(ii == 0)
                        points[ii][jj] = (rand() % (maxx-1))+1;  //random column
                    else
                        points[ii][jj] = (rand() % (maxy-1))+1;  //random random raw
                }
            }
        }

        for(int b=0; b < ncoord; b++){
            
            sprintf(temp, "%.3f,%.3f|", (float)points[0][b], (float)points[1][b]);
            strcat(obststr, temp);
            sprintf(msg, "OBSTACLES: obstcle %d: x=%.3f  y=%.3f\n", b, (float)points[0][b], (float)points[1][b]);
            RegToLog(obstlog, msg);
        }
        obststr[strlen(obststr)-1] = '\0';  //remove the last |
        RegToLog(obstlog, obststr);

        //send the obstacles
        
        Send(sock, obststr, obstlog);

        struct timeval timeout;
        timeout.tv_sec = 40;
        timeout.tv_usec = 0;
        FD_ZERO(&readfd);
        FD_SET(sock, &readfd);
        do{
            sel = select(sock+1, &readfd, NULL, NULL, &timeout);
        }while(sel<0 && errno == EINTR);
        if(sel <0){
            RegToLog(error, "OBSTACLE: error in select");
            perror("select");
            fclose(obstlog);
            fclose(routine);
            fclose(error);
            exit(EXIT_FAILURE);
        }
        else if(sel>0){
            if(FD_ISSET(sock, &readfd)){
                RegToLog(obstlog, "reding message");
                char buffer[MAX_MSG_LEN];
                Recieve(sock, buffer, obstlog);
                if(strcmp(buffer, stop) == 0){
                    RegToLog(obstlog, "stop message recieved");
                    stoprec = true;
                }
            }
        }
        else
            RegToLog(obstlog, "timeout expired");
        
    }

    RegToLog(obstlog, "exiting return value 0");
    pthread_mutex_destroy(&socket_mutex);
    //close the files
    close(sock);
    fclose(obstlog);
    fclose(routine);
    fclose(error);
    
    return 0;
}