#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/ipc.h>
#include  <sys/shm.h>
#include  <unistd.h>
#include  <pthread.h>
#include  <sys/stat.h>

#include  "header.h"

#define BUF_SIZE 1024
#define SHM_SIZE 512

struct Memory  *ShmPTR; // globalize Shared memory pointer
pthread_mutex_t lock;

void *recvData(void *arg) // monitor for the server's input
{
    ShmPTR = arg;
    while (ShmPTR->quit == 0){
        for (int i = 0; i < 10; i ++){
            if (ShmPTR->serverflag[i] == FILLED){
                printf("Slot %d: %d\n", i + 1, ShmPTR->slot[i]); // print each data
                ShmPTR->serverflag[i] = TAKEN;
            }
        }
    }

    return NULL;
}


void *monitorProgress(void *arg)
{
    ShmPTR = arg;
    while(ShmPTR->quit == 0) // print progress consitently
    {
        pthread_mutex_lock(&lock);
        sleep(1);
        printProgress();
        pthread_mutex_unlock(&lock);
    }

    return NULL;
}

void printProgress() // print progress
{
    for (int i = 0; i < 10; i++)
    {
        if (ShmPTR->queryflag[i] != NOT_READY)
        {
            printf("Q%d: ", i + 1);
            for (int j = 0; j < 10; j++)
            {
                printf("%c", ShmPTR->progress[i][j]);
            }
            printf(" ");
            if (((i + 1) % 5 == 0) && (i != 0))
            {
                printf("\n");
            }
        }
    }
    printf("\n");
}

void* printTime(void *arg) // print the processing time
{
    int index = arg;
    clock_t begin;
    clock_t end;

    double time_spent = 0.0;
    begin = clock(); // timer starts

    while(ShmPTR->quit == 0)
    {
        if(ShmPTR->queryflag[index] == TAKEN)
        {
            end = clock(); // timer ends
            time_spent += (double)(end - begin) / CLOCKS_PER_SEC; // calculate the time
            sleep(1);
            printf("**********The query %d was done. The process time was %lf*************\n", index + 1, time_spent);
            ShmPTR->queryflag[index] = NOT_READY;
            break;
        }
    }

    return NULL;
}

int main(int argc, char * argv[])
{
    key_t          ShmKEY;
    int            ShmID;
    char str[256];
    char *eptr;
    unsigned long num;
    
    
    ShmKEY = ftok(".", 'x'); // create a key
    ShmID = shmget(ShmKEY, sizeof(struct Memory), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | IPC_CREAT); // create a ID
    if (ShmID < 0) {
        perror("shmget");
        exit(1);
    }

    ShmID = shmget(ShmKEY, sizeof(struct Memory), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | IPC_CREAT);
    if (ShmID < 0) {
        perror("shmat");
        exit(1);
    }
    
    ShmPTR = (struct Memory *) shmat(ShmID, NULL, 0); // create a pointer
    if ((int) ShmPTR == -1) {
        printf("*** shmat error (server) ***\n");
        exit(1);
    }

    pthread_t *tid = malloc(12 * sizeof(pthread_t));

    if (pthread_mutex_init(&lock, NULL) != 0) { 
        printf("\n mutex init has failed\n");   
        return 1;                               
    }

    pthread_create(&tid[0], NULL, recvData, ShmPTR); // thread for receiving data
    
    printf("Enter the 32-bit interger: \n");

    pthread_create(&tid[1], NULL, monitorProgress, ShmPTR); // thread for monitoring progress

    while (1){
    fgets(str, 256, stdin);
    fflush(stdin);
    fflush(stdout);
    if (str[0] == 'q') // if client quits
    {
        printf("The client is about to close\n");
        ShmPTR->quit = 1;
        pthread_join(tid[0], NULL);
        pthread_join(tid[1], NULL);
        for (int i = 2; i < 12; i++)
        {
            pthread_join(tid[i], NULL);
        }

        free(tid);
        shmdt((void *) ShmPTR);
        printf("client has detached its shared memory...\n");
        shmctl(ShmID, IPC_RMID, NULL);
        printf("client has removed its shared memory...\n");
        pthread_mutex_destroy(&lock);
        printf("client exits...\n");
        exit(0);
    }
    if (ShmPTR->num_process >= 10) // ifb clients send more than 10 requests
    {
        printf("The server is busy\n");
        continue;
    }

    num = strtoul(str, &eptr, 10); // convert it to unsigned long
    ShmPTR->clientflag  = NOT_READY;
    ShmPTR->number = num;
    ShmPTR->clientflag = FILLED;


    while(ShmPTR->clientflag != TAKEN);
    pthread_create(&tid[ShmPTR->counter + 2], NULL, printTime, ShmPTR->counter);
    }
    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    shmdt((void *) ShmPTR);
    printf("client has detached its shared memory...\n");
    shmctl(ShmID, IPC_RMID, NULL);
    printf("client has removed its shared memory...\n");
    printf("client exits...\n");
    exit(0);

    return 0;
}
