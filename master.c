#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
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

pthread_t thread1, thread2;

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

//function to start the programs and return the pid
int spawn(const char * program, char ** arg_list){
    FILE* error = fopen("files/error.log", "a");
    pid_t child_pid = fork();
    if(child_pid != 0)
        return child_pid;
    else{
        execvp(program, arg_list);
        perror("exec failed");
        RegToLog(error, "MASTER: execvp failed");
        exit(EXIT_FAILURE);
    }
    fclose(error);
}

void* spawn_multithreads(void* args){
    FILE* error = fopen("files/error.log", "a");

    char ** info = (char**)args;

    pid_t pid = fork();
    if(pid < 0){
        perror("fork thread");
        RegToLog(error, "MASTER: error in threads");
        pthread_exit(NULL);
    }
    if(pid == 0){
        execvp(info[0], info);
        perror("execvp threads");
        exit(EXIT_FAILURE);
    }
    fclose(error);
    pthread_exit(NULL);
}

void clear_inputbuffer(){
    int c;
    while((c = getchar()) != '\n' && c != EOF){

    }
}

int main(int argc, char* argv[]){

    FILE* routine = fopen("files/routine.log", "a");
    FILE* error = fopen("files/error.log", "w");
    FILE* pidlog = fopen("files/pidlog.log", "a");

    if(error == NULL || routine == NULL || pidlog == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    
    RegToLog(routine, "MASTER : started");

    //pid for the programs
    pid_t server, drone, key, watchdog, obst, target;
    int portserv = 3500;
    int portT_O = 3500;
    char portstrse[10];
    char portstrto[10];
    char ipAdrress[20] = "127.0.0.1";
    int nprc = 0;

    //pthread_t thread1, thread2;

    int pipe_sd[2]; //pipe from server to drone
    if(pipe(pipe_sd) == -1){
        perror("error in pipe_sd");
        RegToLog(error, "MASTER : error in opening pipe_sd");
    }

    char piperd[10]; //readable pipe for server-drone
    char pipewr[10]; //writtable pipe for server-drone
    
    sprintf(piperd, "%d", pipe_sd[0]);
    sprintf(pipewr, "%d", pipe_sd[1]);

    int pipe_sd3[2]; //pipe drone-key
    if(pipe(pipe_sd3) == -1){
        perror("error in pipe_sd");
        RegToLog(error, "MASTER : error in opening pipe_sd");
    }

    char piperd3[10]; //readable pipe drone-key
    char pipewr3[10]; //writtable pipe drone-key
    
    sprintf(piperd3, "%d", pipe_sd3[0]);
    sprintf(pipewr3, "%d", pipe_sd3[1]);

    int pipe_sd4[2]; //pipe from server to keyboard in case the drone is too close to an obstacle
    if(pipe(pipe_sd4) == -1){
        perror("error in pipe_sd4");
        RegToLog(error, "MASTER : error in opening pipe_sd4");
    }

    char piperd4[10]; //readable pipe from server to keyboard 
    char pipewr4[10]; //writtable pipe from server to keyboard 
    
    sprintf(piperd4, "%d", pipe_sd4[0]);
    sprintf(pipewr4, "%d", pipe_sd4[1]);

    sprintf(portstrse, "%d", portserv);
    sprintf(portstrto, "%d", portT_O);

    //process path
    char * drone_path[] = {"./drone", piperd, piperd3, pipewr, NULL};
    char * key_path[] = {"./keyboard", pipewr3, piperd3, piperd4, NULL};
    char * server_path[] = {"./server", pipewr, piperd, pipewr4, portstrse, NULL};
    char * obstacles_path[] = {"./obstacles", portstrto, ipAdrress, NULL};
    char * target_path[] = {"./target", portstrto, ipAdrress, NULL};

    int i = 0;
    
    //bool all = false;
    char button;

    while(i < 2){  //just gives the initial information for the application
        if(i == 0){
            printf("\t\tWELCOME TO DRONE SIMULATOR BY Tommaso De Angeli\n\n");
            printf("\t\tthird assignment of Advance and Robot Programming\n\n");
            sleep(0.5);
            //printf("press q to stop the simulation or any other button to continue...\n\n\n\n");
            printf("a -> to start the simulation\n");
            printf("q -> to to end\n");
            //clear_inputbuffer();
            button = getchar();
            if(button == 'q'){
                RegToLog(routine, "MASTER : end by user");
                fclose(error);
                fclose(routine);
                fclose(pidlog);
                exit(EXIT_FAILURE);
            }
            if(button == 'a'){
                printf("\t\tTHE COMPLETE SIMULATION UPLOADED...\n");
                //all = true;
                i ++;
            }
        }

        if(i == 1){
            printf("\t\tKEYS INSTRUCTIONS\n");
            printf("\tUP 'e'\n");
            printf("\tUP_LEFT 'w'\n");
            printf("\tUP_RIGHT 'r'\n");
            printf("\tRIGHT 'f'\n");
            printf("\t0 FORCES 'd'\n");
            printf("\tLEFT 's'\n");
            printf("\tDOWN 'c'\n");
            printf("\tDOWN_LEFT 'x'\n");
            printf("\tDOWN_RIGHT 'v'\n");
            printf("\tQUIT 'q'\n\n\n");
            printf("\t\tOK, LET'S START!!\n");

            sleep(4);
            i++;
                
        }
        
    }

    
    nprc = 3;
    server = spawn("./server", server_path);
    usleep(500000);
    key = spawn("./keyboard", key_path);
    usleep(500000);
    drone = spawn("./drone", drone_path);
    sleep(4);
    /*obst = spawn("./obstacles", obstacles_path);
    usleep(500000);
    target = spawn("./target", target_path);
    usleep(500000);*/
    if(pthread_create(&thread1, NULL, spawn_multithreads, (void*)obstacles_path) != 0){
        perror("pthread create for obst");
        RegToLog(error, "MASTER: error in creating thread for obst");
        fclose(error);
        fclose(routine);
        fclose(pidlog);
        exit(EXIT_FAILURE);
    }
    sleep(1);
    if(pthread_create(&thread2, NULL, spawn_multithreads, (void*)target_path) != 0){
        perror("pthread create for tar");
        RegToLog(error, "MASTER: error in creating thread for tar");
        fclose(error);
        fclose(routine);
        fclose(pidlog);
        exit(EXIT_FAILURE);
    }
    usleep(500000);

    pthread_detach(thread1);
    pthread_detach(thread2);

    pid_t pids[] = {server, drone, key};
    char pidsstring[3][50];

    //insert all pids inside the pidsstring
    for(size_t i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
        sprintf(pidsstring[i], "%d", pids[i]);
    }

    //put in the watchdog path all the pids of the other processes
    char* watch_path[] = {"./watchdog", pidsstring[0], pidsstring[1], pidsstring[2], NULL};
    sleep(1);
    watchdog = spawn("./watchdog", watch_path);
    usleep(500000);

    fprintf(pidlog, "server_pid:%d\ndrone_pid:%d\nkeyboard_pid:%d", server, drone, key);
    fflush(pidlog);

    kill(server, 34);
    usleep(500000);
    kill(drone, 34);
    usleep(500000);
    kill(key, 34);
    usleep(500000);

    //wait the finish of all the processes
    for(int n = 0; n < (nprc + 1); n++){
        wait(NULL);
    }
    
    RegToLog(routine, "MASTER : finish");

    //close the pipes
    close(pipe_sd[0]);
    close(pipe_sd[1]);
    close(pipe_sd3[0]);
    close(pipe_sd3[1]);
    close(pipe_sd4[0]);
    close(pipe_sd4[1]);

    //close the file
    fclose(error);
    fclose(routine);
    fclose(pidlog);

    return 0;
}
