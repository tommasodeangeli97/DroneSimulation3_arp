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
    FILE* str_log = fopen("files/select_tar_recieve.log", "a");
    RegToLog(str_log, "TAR: recieve start");
    int error_sock = 0;
    socklen_t len = sizeof(error_sock);
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
        int retval = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error_sock, &len);
        if (retval != 0) {
            RegToLog(debug, "errore nella chiamata getsockopt");
        }
        if (error_sock != 0) {
            // Ci sono stati errori nel socket
            fprintf(debug, "Socket error: %s\n prima di recv in Recieve", strerror(error_sock));
            // Gestisci l'errore, ad esempio chiudendo il socket o tentando di riconnettere
        }
        do{
            sel2 = select(sockfd+1, &read_fd, NULL, NULL, &timeout3);
        }while(sel2<0 && errno == EINTR);
        if(sel2 < 0){
            RegToLog(str_log, "tar :: error in select nel recieve");
            perror("select2");
            fclose(str_log);
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
                RegToLog(str_log, "TARGET: SOCKETSERVER error recieving message frome client");
                fclose(str_log);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta recv della Recieve");
            pthread_mutex_unlock(&socket_mutex);
            ok = true;
        }
        /*else
            RegToLog(str_log, "tar :: select1 in the recieve function = 0");*/
    }
    ok = false;
    RegToLog(debug, buffer);
    RegToLog(debug, "tar :: in the riecieve func after recv");
    while(!ok){
        FD_ZERO(&write_fd);
        FD_SET(sockfd, &write_fd);
        int sel;
        int retval = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error_sock, &len);
        if (retval != 0) {
            RegToLog(debug, "errore nella chiamata getsockopt");
        }
        if (error_sock != 0) {
            // Ci sono stati errori nel socket
            fprintf(debug, "Socket error: %s\n prima di send in Recieve", strerror(error_sock));
            // Gestisci l'errore, ad esempio chiudendo il socket o tentando di riconnettere
        }
        do{
            sel = select(sockfd+1, NULL, &write_fd, NULL, &timeout3);
        }while(sel<0 && errno == EINTR);
        if(sel < 0){
            RegToLog(str_log, "tar :: error in select nel recieve 2");
            perror("select");
            fclose(str_log);
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
                RegToLog(str_log, "TARGET: SOCKETSERVER error in sending message to client");
                fclose(str_log);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta send della Recieve");
            pthread_mutex_unlock(&socket_mutex);
            ok=true;
        }
        /*else
            RegToLog(str_log, "tar :: select in the recieve function = 0");*/
    }
    //pthread_mutex_unlock(&socket_mutex);
    fclose(str_log);
}

void Send(int sock, char *msg, FILE *debug){
    //function to send a message and recieve an echo
    FILE* sts_log = fopen("files/select_tar_send.log", "a");
    int error_sock = 0;
    socklen_t len = sizeof(error_sock);
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
        int retval = getsockopt(sock, SOL_SOCKET, SO_ERROR, &error_sock, &len);
        if (retval != 0) {
            RegToLog(debug, "errore nella chiamata getsockopt");
        }
        if (error_sock != 0) {
            // Ci sono stati errori nel socket
            fprintf(debug, "Socket error: %s\n prima di send in Send", strerror(error_sock));
            // Gestisci l'errore, ad esempio chiudendo il socket o tentando di riconnettere
        }
        do{
            sel = select(sock+1, NULL, &write_fd, NULL, &timeout3);
        }while(sel<0 && errno == EINTR);
        if(sel < 0){
            RegToLog(sts_log, "tar: error in select nel send 1");
            perror("select");
            fclose(sts_log);
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
                RegToLog(sts_log, "TARGET: error in sending message to server");
                fclose(sts_log);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta send della Send");
            pthread_mutex_unlock(&socket_mutex);
            ok = true;
        }
        /*else
            RegToLog(sts_log, "tar :: select in the send function = 0");*/
    }
    ok = false;
    //sleep(1);
    RegToLog(debug, "tar :: in the send function after send()");

    while(!ok){
        
        FD_ZERO(&read_fd);
        FD_SET(sock, &read_fd);
        int sel2;
        //RegToLog(debug, "una scritta di merda");
        int retval = getsockopt(sock, SOL_SOCKET, SO_ERROR, &error_sock, &len);
        if (retval != 0) {
            RegToLog(debug, "errore nella chiamata getsockopt");
        }
        if (error_sock != 0) {
            // Ci sono stati errori nel socket
            fprintf(debug, "Socket error: %s\n prima di recv in Send", strerror(error_sock));
            // Gestisci l'errore, ad esempio chiudendo il socket o tentando di riconnettere
        }
        do{
            sel2 = select(sock+1, &read_fd, NULL, NULL, &timeout3);
        }while(sel2<0 && errno == EINTR);
        if(sel2 <0){
            RegToLog(sts_log, "tar: error in select nel send");
            perror("select2");
            fclose(sts_log);
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
                RegToLog(sts_log, "TARGET: error in recieving message frome the server");
                fclose(sts_log);
                pthread_mutex_unlock(&socket_mutex);
                exit(EXIT_FAILURE);
            }
            else if(prova_peer == 0)
                RegToLog(debug, "connessione interrotta recv della Send");
            pthread_mutex_unlock(&socket_mutex);
            ok = true;
        }
        /*else 
            RegToLog(sts_log, "tar :: select in the send function serversock =0");*/
    }

    RegToLog(debug, "Message echo:");
    RegToLog(debug, recvmsg);
    if(strcmp(recvmsg, msg) != 0){
        RegToLog(sts_log, "TARGET: echo not right");
        fclose(sts_log);
        exit(EXIT_FAILURE);
    }
    //pthread_mutex_unlock(&socket_mutex);
    fclose(sts_log);
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

    if(error == NULL || routine == NULL || tarlog == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    
    //pthread_mutex_init(&mutex, NULL);
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
        fclose(error);
        fclose(routine);
        fclose(tarlog);
        exit(EXIT_FAILURE);
    }
    int lock_file = flock(fp, LOCK_SH);
    if(lock_file == -1){
        perror("failed to lock the file pid");
        RegToLog(error, "TARGET; error in lock the failmsmsms");
        fclose(error);
        fclose(routine);
        fclose(tarlog);
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
    close(fp);
    fprintf(tarlog, "n_tar: %d  \n", ncoord);
    fflush(tarlog);

    char msg[100];  //to write on the logfile
    
    struct sockaddr_in server_address;

    struct hostent *server;
    int port = 3500;  //default port
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
    //FD_ZERO(&readfd);

    sscanf(argv[1], "%d", &port);
    sprintf(msg, "TARGET: port = %d", port);
    RegToLog(tarlog, msg);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1){
        perror("socket");
        RegToLog(error, "TARGET: error in creating the socket");
        return 1;
    }
    sprintf(msg, "TARGET: sock value = %d", sock);
    RegToLog(tarlog, msg);

    bzero((char*)&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    //server_address.sin_addr.s_addr = inet_addr("10.42.0.1");

    //convert string in ip address
    //server_address.sin_addr.s_addr = inet_addr(argv[2]);
    if((inet_pton(AF_INET, argv[2], &server_address.sin_addr)) <= 0){
        perror("inet_pton");
        RegToLog(error, "TARGET: error in inet_pton()");
        return 1;
    }
    sprintf(msg, "TARGET: ip address = %d", server_address.sin_addr.s_addr);
    RegToLog(tarlog, msg);

    //connect to server
    if((connect(sock, (struct sockaddr*)&server_address, sizeof(server_address))) < 0){
        perror("connect");
        RegToLog(tarlog, "TARGET: error in connecting to server");
        RegToLog(error, "TARGET: error in connecting to server");
        return 1;
    }
    RegToLog(tarlog, "connected to serverSocket");

    struct timeval timeout;
    timeout.tv_sec = 4;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    memset(sockmsg, '\0', MAX_MSG_LEN);
    srand(time(NULL));  //initialise random seed
    Send(sock, message, tarlog);
    
    RegToLog(tarlog, "TARGET: message sent");
    //recienve dimentions of the window
    //sleep(3);
    Recieve(sock, sockmsg, tarlog);
    RegToLog(tarlog, "TARGET: starting message recieved");
            
    char *format = "%f,%f";
    sscanf(sockmsg, format, &r, &c);
    maxx = (int)r;
    maxy = (int)c;
    fprintf(tarlog, "\n x:%d  y:%d\n", maxx, maxy);
    fflush(tarlog);
            
    int points[2][ncoord];
    //sleep(2);
    
    //sending the coordinates untill the server gives back the stop signal
    sleep(1);
    while(!stoprec){
        
        RegToLog(tarlog, "entering in the loop");
        gecheck = false;
        
        char targetstr[MAX_MSG_LEN];
        char temp[50];
        
        //add the number of targets to the string
        sprintf(targetstr, "T[%d]", ncoord);
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
            strcat(targetstr, temp);
            sprintf(msg, "TAREGET: targets %d: x=%.3f  y=%.3f\n", b, (float)points[0][b], (float)points[1][b]);
            RegToLog(tarlog, msg);
        }
        targetstr[strlen(targetstr)-1] = '\0';  //remove the last |
        RegToLog(tarlog, targetstr);

        //send the targets
        Send(sock, targetstr, tarlog);
        
        while(!gecheck){
            /*struct timeval timeout;
            timeout.tv_sec = 4;
            timeout.tv_usec = 0;*/
            int sel;
            FD_ZERO(&readfd);
            FD_SET(sock, &readfd);
            do{
                sel = select(sock+1, &readfd, NULL, NULL, &timeout);
                //RegToLog(tarlog, "inside the sel loop");
            }while(sel<0 && errno == EINTR);
            if(sel <0){
                RegToLog(error, "target: error in select");
                perror("select");
                fclose(error);
                fclose(routine);
                fclose(tarlog);
                exit(EXIT_FAILURE);
            }
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
            /*else
                RegToLog(tarlog, "target process select = 0 timeout expired");*/
        }
        
    }
    
    RegToLog(tarlog, "exiting return value 0");
    pthread_mutex_destroy(&socket_mutex);
    //close the files
    close(sock);
    fclose(tarlog);
    fclose(routine);
    fclose(error);
    
    return 0;
}