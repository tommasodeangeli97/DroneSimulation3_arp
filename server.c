#define _POSIX_C_SOURCE 199309L
#include <ncurses.h>
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
#include <semaphore.h>
#include <pthread.h>

#define MAX_LINE_LENGHT 256
#define MAX_MSG_LEN 1024
#define form '%'
#define MAXF 4
#define NCLIENT 2
#define MINIMUM_DISTANCE 0.1

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif

pthread_mutex_t mutex;
bool sigint_rec = false;
pid_t key_pid, drone_pid;
int* pipeOb[2];
int* pipeTar[2];

//quantities for the proximity and repulsive forces
float rho0 = 6; //repulsive force range
float rho2 = 2; //takes obstacles and target range
float eta = 40;

typedef struct{  //shared memory
    int forces[2];
    double vel[2];
    int score;
    int obst;
    int target;
} SharedMemory;

//function to take the max
int max(int a, int b){
    if(a>b)
        return a;
    else
        return b;
}

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

pid_t watch_pid = -1;  //declaration pid of the watchdog
void signalhandler(int signo, siginfo_t* info, void* contex){
    if(signo == SIGUSR1){  //SIGUSR1 
        FILE* routine = fopen("files/routine.log", "a");
        FILE* error = fopen("files/error.log", "a");
        watch_pid = info->si_pid;  //initialisation watchdog's pid
        if(watch_pid == -1){
            fprintf(error, "%s\n", "DRONE : error in recieving pid");
            fclose(routine);
            fclose(error);
            perror("recieving pid drone");
            exit(EXIT_FAILURE);
        }
        else{
            fprintf(routine, "%s\n", "SERVER : started success");
            kill(watch_pid, SIGUSR1);
            fclose(routine);
            fclose(error);
        }
    }

    if(signo == SIGUSR2){
        FILE* routine = fopen("files/routine.log", "a");
        fprintf(routine, "%s\n", "SERVER : program terminated by WATCHDOG");
        fclose(routine);
        exit(EXIT_FAILURE);
    }
    if(signo == SIGINT){
        //printf("server terminating return 0");
        FILE* routine = fopen("files/routine.log", "a");
        FILE* error = fopen("files/error.log", "a");
        fprintf(routine, "%s\n", "SERVER : terminating");
        fclose(routine);
        sigint_rec = true;
        if(write(*pipeOb[1], "STOP", strlen("STOP")) == -1){
            perror("error in writing stop to obst");
            RegToLog(error, "SERVER: error in writing stop to obst");
            exit(EXIT_FAILURE);
        }
        fsync(*pipeOb[1]);
        sleep(1);
        if(write(*pipeTar[1], "STOP", strlen("STOP")) == -1){
            perror("error in writing stop to tar");
            RegToLog(error, "SERVER: error in writing stop to tar");
            exit(EXIT_FAILURE);
        }
        fsync(*pipeTar[1]);
        fclose(error);
        sleep(1);
    }

    if(signo == 34){  //signal to collect the pids of the others processes

        FILE* serverlog = fopen("files/server.log", "a");

        int fd;
        const char* logfile = "files/pidlog.log";
        char pidline[MAX_LINE_LENGHT];
        fd = open(logfile, O_RDONLY);
        if(fd == -1){
            perror("fp opening");
            fprintf(serverlog, "error in fd");
            exit(EXIT_FAILURE);
        }

        int lock_file = flock(fd, LOCK_SH);
        if(lock_file == -1){
            perror("failed to lock the file pid");
            fprintf(serverlog, "error in lock");
            exit(EXIT_FAILURE);
        }

        FILE* f = fdopen(fd, "r");
        
        int b = 0;
        while(fgets(pidline, sizeof(pidline), f) != NULL){
            char label[MAX_LINE_LENGHT];
            int value;
            if(sscanf(pidline, "%[^:]:%d", label, &value) == 2){
                if(strcmp(label, "keyboard_pid") == 0){
                    key_pid = value;
                    b++;
                }
                if(strcmp(label, "drone_pid") == 0){
                    drone_pid = value;
                    b++;
                }
                if(b>=2)
                    break;
            }
            else{
                fprintf(serverlog, "problems in the pid acquisation");
            }
        }

        int unlock_file = flock(fd, LOCK_UN);
        if(unlock_file == -1){
            perror("failed to unlock the file pid");
        }
        fclose(f);
        close(fd);
        fprintf(serverlog, "keyboard_pid:%d , drone_pid:%d\n", key_pid, drone_pid);
        fclose(serverlog);
    }
}

//function to check if the obstacles and the targets are coincident
int check_ostar(int ox, int oy, int tx, int ty){
    if(ox == tx && oy == ty){
        FILE* ferr = fopen("files/error.log", "a");
        fprintf(ferr, "%s\n", "SERVER: coincident points obstacles-target");
        fclose(ferr);
        return 1;
    }
    else
        return 0;
}

//function to check id the drone is near to a target
int near(int cx1, int cy1, int cx2, int cy2){
    float rho = sqrt(pow(cx1-cx2, 2)+pow(cy1-cy2, 2));
    if(rho < rho2)
        return 1;
    return 0;
}

//function to check if the drone is near to an obstacle and evalueates the new force acting on the drone
float near_obst(int cx1, int cy1, int cx2, int cy2, char direction) {
    // Initialize force components to 0
    float forcerep[2] = {0};

    // Calculate the distance (rho) between the drone (cx1, cy1) and the obstacle (cx2, cy2)
    float dx = cx1 - cx2;
    float dy = cy1 - cy2;
    float rho = sqrt(dx * dx + dy * dy);  // Distance between the two points

    // Only calculate repulsive force if within the perception radius
    if (rho < rho0 && rho > MINIMUM_DISTANCE) {
        // Calculate the repulsive force magnitude based on the inverse square law
        float repulsion_strength = (1.0 / (rho * rho)) - (1.0 / (rho0 * rho0));

        // Calculate the normalized direction vector from obstacle to drone
        float direction_x = dx / rho;
        float direction_y = dy / rho;

        // Calculate the force components along the x and y directions
        forcerep[0] = repulsion_strength * direction_x;
        forcerep[1] = repulsion_strength * direction_y;

        // Cap the forces if they exceed the maximum allowed force
        float total_force = sqrt(forcerep[0] * forcerep[0] + forcerep[1] * forcerep[1]);
        if (total_force > MAXF) {
            // Scale the forces to ensure the total force does not exceed MAX_FORCE
            float scaling_factor = MAXF / total_force;
            forcerep[0] *= scaling_factor;
            forcerep[1] *= scaling_factor;
        }
    }
    // If the distance is less than the minimum, apply maximum repulsive force directly
    if (rho <= MINIMUM_DISTANCE) {
        // Max force in the direction from obstacle to drone
        float direction_x = dx / rho;
        float direction_y = dy / rho;
        forcerep[0] = MAXF * direction_x;
        forcerep[1] = MAXF * direction_y;
    }
    if(direction == 'x')
        return forcerep[0];  // Return the calculated force components
    else if(direction == 'y')
        return forcerep[1];
}

void* spawn_multithreads(void* args){
    FILE* error = fopen("files/error.log", "a");

    char ** info = (char**)args;

    pid_t pid = fork();
    if(pid < 0){
        perror("fork thread");
        RegToLog(error, "MASTER: error in threads");
        fclose(error);
        pthread_exit(NULL);
    }
    if(pid == 0){
        execvp(info[0], info);
        perror("execvp threads");
        fclose(error);
        exit(EXIT_FAILURE);
    }
    fclose(error);
    pthread_exit(NULL);
}

bool between(int cix, int ciy, int cox, int coy, int cfx, int cfy){
    return (cox >= cix && cox <= cfx || cox <= cix && cox >= cfx) && (coy >= ciy && coy <= cfy || coy <= ciy && coy >= cfy);
}

int main(int argc, char* argv[]){
    //initialization of ncurses
    initscr();

    FILE* routine = fopen("files/routine.log", "a");
    FILE* error = fopen("files/error.log", "a");
    FILE* serverlog = fopen("files/server.log", "a");

    if(error == NULL || routine == NULL || serverlog == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }    

    if(has_colors()){
        start_color();  //enables the color
        init_pair(1, COLOR_BLUE, COLOR_WHITE);  //define the window color
        init_pair(2, COLOR_RED, COLOR_WHITE); //define the obstacle color
        init_pair(3, COLOR_GREEN, COLOR_WHITE); //define the target color
    }

    //initialise the mutex
    if(pthread_mutex_init(&mutex, NULL) != 0){
        perror("mutex");
        RegToLog(error, "SERVER : error in mutex creation");
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }

    SharedMemory *sm;  //shared memory pointer
    //shared memory opening and mapping
    const char * shm_name = "/shared_memory";
    const int SIZE = 4096;
    int i, shm_fd;
    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if(shm_fd == 1){
        perror("shared memory faild\n");
        RegToLog(error, "SERVER : shared memory faild");
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }
    else{
        //printf("SERVER : created the shared memory");
        RegToLog(serverlog, "SERVER : created the shared memory");
    }

    if(ftruncate(shm_fd, SIZE) == 1){
        perror("ftruncate");
        RegToLog(error, "SERVER : ftruncate faild");
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }

    sm = (SharedMemory *)mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(sm == MAP_FAILED){
        perror("map failed");
        RegToLog(error, "SERVER : map faild");
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }

    //semaphore opening
    sem_t * sm_sem;
    sm_sem = sem_open("/sm_sem1", O_CREAT | O_RDWR, 0666, 1);
    if(sm_sem == SEM_FAILED){
        RegToLog(error, "SERVER : semaphore faild");
        perror("semaphore");
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;  //initialize sigaction
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;  //use sigaction field instead of signalhandler
    sa.sa_sigaction = signalhandler;

    if(sigaction(SIGUSR1, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "SERVER: error in sigaction()");
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGUSR2, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "SERVER : error in sigaction()");
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGINT, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "SERVER : error in sigaction()");
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }
    if(sigaction(34, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "SERVWR : error in sigaction()");
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }

    //pipe to give to the drone, the obstacles and the target the max x and y
    int writesd;
    fd_set read_fd;
    fd_set write_fd;
    FD_ZERO(&read_fd);
    FD_ZERO(&write_fd);

    writesd = atoi(argv[1]);

    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);  //takes the max number of rows and colons
    bool okdrone = false;
    
    write(writesd, &max_x, sizeof(int));
    fsync(writesd);
                
    sleep(1);
                
    write(writesd, &max_y, sizeof(int));
    fsync(writesd);
                
    sleep(1);

    int n_obst = 0, n_tar = 0;
    char ti[] = "TI";
    char oi[] = "OI";
    pid_t pidcli[NCLIENT];
    int port = 40000;
    //int client_sock;
    char msg[100];
    char sockmsg[MAX_MSG_LEN];
    char *token;
    char stop[] = "STOP";
    char ge[] = "GE";
    char start[] = "START";
    bool memorytarget = false;
    char fd_str[10];
    int opt = 1;
    char piperd[4][12];
    char pipewr[4][12];
    int pipe_fd[4][2];

    sscanf(argv[4], "%d", &port);
    sprintf(msg, "SERVER: port value = %d", port);
    RegToLog(serverlog, msg);

    srand(time(NULL));  //initialise random seed

    int droneposx, droneposy;
    int droneforx, dronefory;

    droneposx = rand() % max_x;  //random column
    droneposy = rand() % max_y;  //random row
    if(droneposx <= 1)
        droneposx = 2;
    if(droneposx >= max_x-1)
        droneposx = max_x-2;
    if(droneposy <= 1)
        droneposy = 2;
    if(droneposy >= max_y)
        droneposy = max_y-1;

    write(writesd, &droneposx, sizeof(int));
    fsync(writesd);
    sleep(1);
    write(writesd, &droneposy, sizeof(int));
    fsync(writesd);
    sleep(1);

    int drone_prev_x = droneposx;
    int drone_prev_y = droneposy;

    int readsd;
    int varre;
    readsd = atoi(argv[2]);

    WINDOW *win = newwin(max_y, max_x, 0, 0);  //creats the window
    box(win, 0, 0);  //adds the borders
    wbkgd(win, COLOR_PAIR(1));  //sets the color of the window
    wrefresh(win);  //prints and refreshes the window

    struct sockaddr_in server_address;
    int addrlen = sizeof(server_address);
    memset(&server_address, 0, sizeof(server_address));

    //generating the socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1){
        perror("socket");
        return 1;
    }
    sprintf(msg, "server: sock value = %d", sock);
    RegToLog(serverlog, msg);
    
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0){
        perror("setsockport");
        RegToLog(error, "SERVER: error in setsockport");
        close(sock);
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }
    int buff_sock_size = 65536;
    if(setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buff_sock_size, sizeof(buff_sock_size)) < 0){
        perror("setsockport");
        RegToLog(error, "SERVER: error in setsockport buff rec size");
        close(sock);
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }
    if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buff_sock_size, sizeof(buff_sock_size)) < 0){
        perror("setsockport");
        RegToLog(error, "SERVER: error in setsockport  buff send size");
        close(sock);
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }
    struct timeval timeout;
    timeout.tv_sec = 4;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    RegToLog(serverlog, "setsockopt ok");

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind the socket
    RegToLog(serverlog, "binding the socket");

    if(bind(sock, (struct sockaddr*)&server_address, sizeof(server_address)) == -1){
        perror("bind");
        RegToLog(error, "SERVER: error in bind");
        return 1;
    }

    //listen for the connection
    if(listen(sock, 5) == -1){  //5 is the length of the queue
        perror("listen");
        return 1;
    }
    RegToLog(serverlog, "socket listening");

    for(int i = 0; i < 4; i++){
        if(pipe(pipe_fd[i]) == -1){
            perror("pipes");
            RegToLog(error, "SERVER: error in pipes");
        }
    }

    for(int i = 0; i< 4; i++){
        sprintf(piperd[i], "%d", pipe_fd[i][0]);
        sprintf(pipewr[i], "%d", pipe_fd[i][1]);
    }
    //sleep(1);

    //generate the server client one for target and one for obstacle
    for(int i = 0; i < NCLIENT; i++){
        pthread_t thread;
        int client_sock;
        do{
            client_sock = accept(sock, (struct sockaddr*)&server_address, &addrlen);
            RegToLog(serverlog, "try");
            sprintf(msg, "server: client_sock value = %d", client_sock);
            RegToLog(serverlog, msg);
            //sleep(1);
        }while (client_sock == -1 && errno == EINTR);
        if( client_sock == -1){
            perror("accept");
            RegToLog(error, "SERVER: error in accept");
            return 1;
        }
        RegToLog(serverlog, "connection accepted");

        sprintf(fd_str, "%d", client_sock);
        char id[5];
        sprintf(id, "%d", i);
        char rc[100];
        sprintf(rc, "%d.000,%d.000", max_x, max_y);

        char* args[] = {"./sockserver", fd_str, piperd[i*2], pipewr[i*2+1], id, rc, NULL};
        
        if(pthread_create(&thread, NULL, spawn_multithreads, (void*)args) != 0){
            perror("pthread create for obst");
            RegToLog(error, "MASTER: error in creating thread for obst");
            fclose(routine);
            fclose(error);
            fclose(serverlog);
            exit(EXIT_FAILURE);
        }
        
        if (pthread_detach(thread) != 0) {
            perror("Failed to detach thread");
            RegToLog(error, "MASTER: error in detach treads");
            exit(EXIT_FAILURE);
        }
        sleep(1);
        RegToLog(serverlog, "forked");
        close(client_sock);
        close(pipe_fd[i*2][0]);
        close(pipe_fd[i*2+1][1]);
        

    }
    //sleep(1);

    //reading the pipes throught the sockets
    for(int i = 0; i < NCLIENT; i++){
        char buffer[MAX_MSG_LEN];
        if(read(pipe_fd[i*2+1][0], buffer, MAX_MSG_LEN) == -1){
            perror("reading from child1");
            RegToLog(error, "SERVER: error in readind from child1");
            fclose(routine);
            fclose(error);
            fclose(serverlog);
            exit(EXIT_FAILURE);
        }
        RegToLog(serverlog, buffer);
        if(strcmp(buffer, ti) == 0){  //target initiasiled
            pipeTar[0] = &pipe_fd[i*2+1][0];
            pipeTar[1] = &pipe_fd[i*2][1];
        }
        if(strcmp(buffer, oi) == 0){
            pipeOb[0] = &pipe_fd[i*2+1][0];
            pipeOb[1] = &pipe_fd[i*2][1];
        }
    }

    if(write(writesd, start, strlen(start)+1) == -1){
        perror("writing to pipe");
        RegToLog(error, "SERVER: error in write to drone");
        fclose(routine);
        fclose(error);
        fclose(serverlog);
        exit(EXIT_FAILURE);
    }
    fsync(writesd);
    sleep(1);
    RegToLog(serverlog, "start command sent to the drone");

    //variables to store the obstacles and target position
    int obst[2][20] = {0};
    int target[2][20] = {0};

    //int sel;
    int count = 0;
    float n1, n2;
    

    int r = 0, score = 0, obt = 0, trt = 0;
    int memo[2][1] = {-1};
    
    bool ok1, okobst, oktar = false;
    bool ok2 = false;
    int n = 0;

    int max_fd = -1;
    
    /*struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;*/

    while(!sigint_rec){
        //pthread_mutex_lock(&mutex);
        if(r >0){
            //deleting the old drone's podition
            wattr_on(win, COLOR_PAIR(1), NULL);
            mvwprintw(win, droneposy, droneposx, " ");  //prints the drone
            wattr_off(win, COLOR_PAIR(1), NULL);
            wrefresh(win);
            
            r--;
        }

        /*if(!memorytarget && !ok2){
            FD_ZERO(&read_fd);
            FD_SET(*pipeOb[0], &read_fd);
            FD_SET(*pipeTar[0], &read_fd);
            
            max_fd = *pipeTar[0] > *pipeOb[0] ? *pipeTar[0] : *pipeOb[0];
        }
        else if(memorytarget && !ok2){
            FD_ZERO(&read_fd);
            FD_SET(*pipeOb[0], &read_fd);

            max_fd = *pipeOb[0];
        }
        else if(!memorytarget && ok2){
            FD_ZERO(&read_fd);
            FD_SET(*pipeTar[0], &read_fd);

            max_fd = *pipeTar[0];
        }
        else if(memorytarget && ok2){*/
        FD_ZERO(&read_fd);
        FD_SET(*pipeOb[0], &read_fd);
        FD_SET(*pipeTar[0], &read_fd);
        FD_SET(readsd, &read_fd);
        FD_SET(*pipeTar[1], &write_fd);
        max_fd = (readsd > *pipeOb[0] ? (readsd > *pipeTar[0] ? readsd : *pipeTar[0]) : (*pipeOb[0] > *pipeTar[0] ? *pipeOb[0] : *pipeTar[0]));
        //}
        //pthread_mutex_unlock(&mutex);
        int sel;
        do{
            sel = select(max_fd+1, &read_fd, NULL, NULL, &timeout);
            /*sprintf(msg, "sel %d", sel);
            RegToLog(serverlog, msg);*/
        }while (sel == -1 && errno == EINTR );

        if(sel == -1){
            perror("error in select");
            RegToLog(error, "SERVER: error in selct");
            fclose(routine);
            fclose(error);
            fclose(serverlog);
            exit(EXIT_FAILURE);
        }
        else if(sel > 0){
            if(FD_ISSET(*pipeTar[0], &read_fd)){  
                RegToLog(serverlog, "reading pipe targets");
                trt = 0;
                memorytarget = true;
                if(ok2){
                    ok1 = true;
                }
                oktar = true;
                memset(sockmsg, '\0', MAX_MSG_LEN);
                //sleep(1);
                if(read(*pipeTar[0], sockmsg, MAX_MSG_LEN) == -1){  
                    perror("error in reading");
                    RegToLog(error, "SERVER: error in reading");
                    fclose(routine);
                    fclose(error);
                    fclose(serverlog);
                    exit(EXIT_FAILURE);
                }
                RegToLog(serverlog, sockmsg);

                if(sockmsg[0] != 'T'){
                    RegToLog(error, "SERVER: error in file description target");
                    fclose(routine);
                    fclose(error);
                    fclose(serverlog);
                    exit(EXIT_FAILURE);
                }
                else{
                    sscanf(sockmsg, "T[%d]", &n_tar);
                    
                    //reading targets from pipe
                    
                    token = strtok(sockmsg, "]");
                    for(token = strtok(NULL, "|"); token != NULL; token = strtok(NULL, "|")){
                        
                        sscanf(token, "%f,%f", &n1, &n2);

                        target[0][count] = (int)n1;
                        target[1][count] = (int)n2;
                        RegToLog(serverlog, "taken tar");
                        sprintf(msg, "target x->%d  y->%d  count->%d", target[0][count], target[1][count], count);
                        RegToLog(serverlog, msg);
                        RegToLog(serverlog, "printed tar");

                        count++;
                    }
                    if(count != n_tar){
                        RegToLog(serverlog, "error in reading targets(count not mach)");
                    }
                    
                }
                
                count = 0;
                sprintf(msg, "count after iter terminated %d", count);
                RegToLog(serverlog, msg);
            }
            if(FD_ISSET(*pipeOb[0], &read_fd)){  
                RegToLog(serverlog, "reading from the pipe obstacles");
                if(memorytarget){
                    ok1 = true;
                }
                wattr_on(win, COLOR_PAIR(2), NULL);
                for(int g = 0; g < n_obst; g++){
                        
                    mvwprintw(win, obst[1][g], obst[0][g], " ");  //prints the obstacles                        
                }
                wattr_off(win, COLOR_PAIR(2), NULL);
                wrefresh(win);
                okobst = true;
                memset(sockmsg, '\0', MAX_MSG_LEN);
                //sleep(1);
                if(read(*pipeOb[0], sockmsg, MAX_MSG_LEN) == -1){  
                    perror("error in reading");
                    RegToLog(error, "SERVER: error in reading2");
                    fclose(routine);
                    fclose(error);
                    fclose(serverlog);
                    exit(EXIT_FAILURE);
                }
                RegToLog(serverlog, sockmsg);
                //usleep(500000);
                if(sockmsg[0] != 'O'){
                    RegToLog(error, "SERVER: error in file description obstacles");
                    fclose(routine);
                    fclose(error);
                    fclose(serverlog);
                    exit(EXIT_FAILURE);
                }
                else{
                    sscanf(sockmsg, "O[%d]", &n_obst);
                    
                    token = strtok(sockmsg, "]");
                    for(token = strtok(NULL, "|"); token != NULL; token = strtok(NULL, "|")){
                        
                        sscanf(token, "%f,%f", &n1, &n2);

                        obst[0][count] = (int)n1;
                        obst[1][count] = (int)n2;
                        RegToLog(serverlog, "taken obst");
                        sprintf(msg, "obstacle x->%d  y->%d  count->%d", obst[0][count], obst[1][count], count);
                        RegToLog(serverlog, msg);                       
                        RegToLog(serverlog, "printed obst");
                        
                        count++;
                    }
                    
                    if(count != n_obst){
                        RegToLog(serverlog, "error in reading obstacles(count not mach)");
                    }
                
                }
                
                count = 0;
                sprintf(msg, "count after iter terminated %d", count);
                RegToLog(serverlog, msg);
                ok2 = true;
            }
            if(FD_ISSET(readsd, &read_fd)){
                RegToLog(serverlog, "reading from the drone pipe");
                //taking the new drone position
                varre = -1;
                //sleep(1);
                while(varre == -1){
                    varre = read(readsd, &droneposx, sizeof(int));
                    //sleep(1);
                }
        
                varre = -1;
        
                while (varre == -1){   
                    varre = read(readsd, &droneposy, sizeof(int));
                    //sleep(1);
                }

                /*fprintf(serverlog, "posx: %d ,, posy: %d \n", droneposx, droneposy);
                fflush(serverlog);*/
            }
            pthread_mutex_lock(&mutex);
            if(memorytarget && ok2 && ok1){
                RegToLog(serverlog, "checking the sovrepposizioni");
                //check if the pionts are sovrapposed and print the obstacles and target
                n = max(n_obst, n_tar);
                int arr_c[2][n];
                for(int k=0; k<n; k++){
                    arr_c[0][k] = -1;
                    arr_c[1][k] = -1;
                }
                int v =0;
                for(int h=0; h<n_obst; h++){
                    for(int g=0; g<n_tar; g++){
                        if(check_ostar(obst[0][h], obst[1][h], target[0][g], target[1][g])){
                            arr_c[0][v] = h;
                            arr_c[1][v] = g;
                            v++;
                        }
                        /*else
                            v++;*/
                    }
                }
                if(v>0){
                    for(int h = 0; h < n_obst; h++){
                        sprintf(msg, "ENTERING IN THE H LOOP    %d", v);
                        RegToLog(serverlog, msg);
                        int overlying = 0;
                        wattr_on(win, COLOR_PAIR(2), NULL);
                        for(int z = 0; z < v; z++){
                            if (arr_c[0][z] == h){
                                overlying = 1;
                                break;
                            }
                            if(!overlying){
                                sprintf(msg, "obst before print %d   %d", obst[0][h],obst[1][h]);
                                RegToLog(serverlog, msg);
                                
                                mvwprintw(win, obst[1][h], obst[0][h], "X");  //prints the obstacles
                            }
                        }
                        wattr_off(win, COLOR_PAIR(2), NULL);
                        wrefresh(win);
                        //sleep(1);
                    }
                    
                    for(int g= 0; g < n_tar; g++){
                        sprintf(msg, "ENTERING IN THE G LOOP     %d", v);
                        RegToLog(serverlog, msg);
                        int overlying = 0;
                        wattr_on(win, COLOR_PAIR(3), NULL);
                        for(int z = 0; z < v; z++){
                            if (arr_c[1][z] == g){
                                overlying = 1;
                                break;
                            }
                            if(!overlying){
                                sprintf(msg, "target before print %d   %d", target[0][g],target[1][g]);
                                RegToLog(serverlog, msg);
                                
                                mvwprintw(win, target[1][g], target[0][g], "O");  //prints the targets
                            }
                        }
                        wattr_off(win, COLOR_PAIR(3), NULL);
                        wrefresh(win);
                        //sleep(1);
                    }
                    //wrefresh(win);
                }
                else{
                    wattr_on(win, COLOR_PAIR(3), NULL);
                    for(int i = 0; i < n_tar; i++){
                        sprintf(msg, "target before print %d   %d", target[0][i],target[1][i]);
                        RegToLog(serverlog, msg);
                        
                        mvwprintw(win, target[1][i], target[0][i], "O");  //prints the targets
                    }
                    wattr_off(win, COLOR_PAIR(3), NULL);
                    wrefresh(win);
                    //sleep(1);
                    wattr_on(win, COLOR_PAIR(2), NULL);
                    for(int g = 0; g < n_obst; g++){
                        sprintf(msg, "obst before print %d   %d", obst[0][g],obst[1][g]);
                        RegToLog(serverlog, msg);
                        
                        mvwprintw(win, obst[1][g], obst[0][g], "X");  //prints the obstacles                        
                    }
                    wattr_off(win, COLOR_PAIR(2), NULL);
                    wrefresh(win);
                    //sleep(1);
                }
                sprintf(msg, "target and obstacles ok");
                RegToLog(serverlog, msg);
                ok1 = false;
                oktar = false;
                okobst = false;
            }
            else if(memorytarget && (!ok2) && oktar){
                wattr_on(win, COLOR_PAIR(3), NULL);
                for(int i = 0; i < n_tar; i++){
                    sprintf(msg, "target before print %d   %d", target[0][i],target[1][i]);
                    RegToLog(serverlog, msg);
                    
                    mvwprintw(win, target[1][i], target[0][i], "O");  //prints the targets                    
                }
                wattr_off(win, COLOR_PAIR(3), NULL);
                wrefresh(win);
                //sleep(1);
                oktar = false;
            }
            else if(ok2 && (!memorytarget) && okobst){
                wattr_on(win, COLOR_PAIR(2), NULL);
                for(int i = 0; i < n_obst; i++){
                    sprintf(msg, "obst before print %d   %d", obst[0][i],obst[1][i]);
                    RegToLog(serverlog, msg);
                    
                    mvwprintw(win, obst[1][i], obst[0][i], "X");  //prints the obstacles
                }
                wattr_off(win, COLOR_PAIR(2), NULL);
                wrefresh(win);
                //sleep(1);
                okobst = false;
            }
            //wrefresh(win);

            RegToLog(serverlog, "updating the window");            
            wattr_on(win, COLOR_PAIR(1), NULL);
            mvwprintw(win, droneposy, droneposx, "%c", form);  //prints the drone
            wattr_off(win, COLOR_PAIR(1), NULL);
            wrefresh(win);
            //sleep(1);
            sem_wait(sm_sem);
            sm->score = score;
            sm->obst = n_obst - obt;
            sm->target = n_tar - trt;
            
            float vx = sm->vel[0];
            float vy = sm->vel[1];
            sem_post(sm_sem);
            pthread_mutex_unlock(&mutex);
            //check if the drone is near to an obstacles, in that case the repulse forse act and the forces are shared whit the keyboard
            if(ok2){
                for(int f = 0; f<n_obst; f++){
                    float a = near_obst(droneposx, droneposy, obst[0][f], obst[1][f], 'x');
                    float b = near_obst(droneposx, droneposy, obst[0][f], obst[1][f], 'y');
                    if(a != 0.0 || b != 0.0){
                        RegToLog(serverlog, "repulse force acting, writing to keyboard");
                        a = ceil(a);
                        b = ceil(b);
                        int ofx = (int)a;
                        int ofy = (int)b;
                        int writesd4 = atoi(argv[3]);
                        sprintf(msg, "forza repulsiva calcolata a->%d    b->%d", ofx,ofy);
                        RegToLog(serverlog, msg);

                        kill(key_pid, SIGUSR1);
                        sleep(1);
                        
                        write(writesd4, &ofx, sizeof(int));
                        fsync(writesd4);
                        sleep(1);
                        write(writesd4, &ofy, sizeof(int));
                        fsync(writesd4);
                        sleep(1);
                        
                        close(writesd4);
                    }
                }
            }
            pthread_mutex_lock(&mutex);
            n = max(n_tar, n_obst);
            //check if the drone pursuit a gol or touch a obstacles and stores in the memo double array the witch target or obstacle is involved
            for(int c =0; c<n; c++){
                if((ok2 && near(droneposx, droneposy, obst[0][c], obst[1][c]) && c<n_obst) || (ok2 && c<n_obst && between(drone_prev_x, drone_prev_y, obst[0][c], obst[1][c], droneposx, droneposy))){
                    wattr_on(win, COLOR_PAIR(2), NULL);
                    
                    score--;
                    
                    mvwprintw(win, obst[1][c], obst[0][c], " ");  //prints the obstacles
                    wattr_off(win, COLOR_PAIR(2), NULL);
                    wrefresh(win);
                    RegToLog(serverlog, "After mvwprintw obstacle after il nulla");
                    memo[0][0] = c;
                }
                if((memorytarget && near(droneposx, droneposy, target[0][c], target[1][c]) && c<n_tar) || (memorytarget && c<n_tar && between(drone_prev_x, drone_prev_y, target[0][c], target[1][c], droneposx, droneposy))){
                    wattr_on(win, COLOR_PAIR(3), NULL);
                    
                    score++;
                    
                    mvwprintw(win, target[1][c], target[0][c], " ");  //prints the obstacles
                    wattr_off(win, COLOR_PAIR(3), NULL);
                    wrefresh(win);
                    RegToLog(serverlog, "After mvwprintw target after il nulla");
                    memo[1][0] = c;
                }
                else{
                    memo[0][0] = -1;
                    memo[1][0] = -1;
                }
                /*sprintf(msg, "memo: x->%d  y->%d", memo[0][0], memo[1][0]);
                RegToLog(serverlog, msg);*/
                if(memo[0][0] >= 0 || memo[1][0] >= 0){
                    if(memo[0][0] >= 0){
                        RegToLog(routine, "SERVER: obstacle taken");
                        obt++;
                        obst[0][memo[0][0]] = -1;
                        obst[1][memo[0][0]] = -1;
                
                    }

                    if(memo[1][0] >= 0){
                        RegToLog(routine, "SERVER: target taken");
                        trt++;
                        target[0][memo[1][0]] = -1;
                        target[1][memo[1][0]] = -1;
                
                    }

                    memo[0][0] = -1;
                    memo[1][0] = -1;     
                }
            }
            drone_prev_x = droneposx;
            drone_prev_y = droneposy;

            //takes the values from the shared memory and prints them on top of the window
            sem_wait(sm_sem);
            sm->score = score;
            sm->obst = n_obst - obt;
            int obstmanc = sm->obst;
            sm->target = n_tar - trt;
            int tartaken = sm->target;
            int forx = sm->forces[0];
            int fory = sm->forces[1];
            sem_post(sm_sem);
            wattr_on(win, COLOR_PAIR(1), NULL);
            mvwprintw(win, 0, 0, "fx:%d fy:%d vx:%f vy:%f target missing:%d obstacles:%d SCORE:%d -----------------",forx,fory,vx,vy,tartaken,obstmanc,score);
            wattr_off(win, COLOR_PAIR(2), NULL);
            wrefresh(win);
            //sleep(1);
            pthread_mutex_unlock(&mutex);

            sprintf(msg, "trt: %d", trt);
            RegToLog(serverlog, msg);
            if(trt == n_tar && n_tar){
                RegToLog(serverlog, "all targets taken");
                if(write(*pipeTar[1], ge, strlen(ge)) == -1){
                    perror("error in writing ge");
                    RegToLog(error, "SERVER: error in writing ge");
                    fclose(routine);
                    fclose(error);
                    fclose(serverlog);
                    exit(EXIT_FAILURE);
                }
                fsync(*pipeTar[1]);
                sleep(1);
                trt = 0;
                n_tar= 0;
                //memorytarget = false;
            }

            //sleep(1);
            r++;
        }
        
    }

    for(int i = 0; i<NCLIENT;  i++){
        if(close(pipe_fd[i*2+1][0]) == -1){
            perror("closing pipes1");
            RegToLog(error, "SERVER: error in closing pipes 1");
        }
        if(close(pipe_fd[i*2][1]) == -1){
            perror("closing pipes2");
            RegToLog(error, "SERVER: error in closing pipes 2");
        }
    }

    //routine to close the shared memory, the files, the pipes and the semaphore
    
    pthread_mutex_destroy(&mutex);
    sem_close(sm_sem);
    close(readsd);
    close(writesd);
    close(sock);
    memset(sm, 0, SIZE);
    munmap(sm, SIZE);
    if(shm_unlink(shm_name) == 1){
        printf("okok");
        exit(EXIT_FAILURE);
    }
    if(close(shm_fd) == 1){
        perror("close");
        RegToLog(error, "SERVER : close faild");
        exit(EXIT_FAILURE);
    }
    fclose(error);
    fclose(routine);
    fclose(serverlog);
    delwin(win);
    endwin();
    return 0;
}