#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/ipc.h>
#include  <sys/shm.h>
#include  <pthread.h>
#include  <unistd.h>
#include  <sys/stat.h>
#include  <string.h>
#include  <time.h>


#include  "header.h"

#define BACKLOG 10
#define BUF_SIZE 1024
#define SHM_SIZE 100

struct Memory  *ShmPTR; // globalize Shared memory pointer
pthread_mutex_t lock;

void *factorise(void *arg) // pthread function to factorise
{
    ShmPTR = arg;
    const int index = ShmPTR->counter; // store the current index processing
    unsigned long num = ShmPTR->temp;

    while (num % 2 == 0) // check 2 as a factor
    {
        while(ShmPTR->serverflag[index] != TAKEN);
        pthread_mutex_lock(&lock);
        usleep(100); // delay in case the thread is too fast
        ShmPTR->serverflag[index] = NOT_READY;
        ShmPTR->slot[index] = 2; // store it into the slot
        ShmPTR->serverflag[index] = FILLED;
        num /= 2;
        pthread_mutex_unlock(&lock);
    }

    // check if it's a factor from 3
    int f = 3;
    while (f * f <= num)
    {
        while(ShmPTR->serverflag[index] != TAKEN);
        pthread_mutex_lock(&lock);
        usleep(100);
        if (num % f == 0)
        {
        ShmPTR->serverflag[index] = NOT_READY;
        ShmPTR->slot[index] = f;
        ShmPTR->serverflag[index] = FILLED;
        num /= f;
        }
        else{
        f += 2;
        }
        pthread_mutex_unlock(&lock);
    }

    if (num != 1)
    {
        while(ShmPTR->serverflag[index] != TAKEN);
        pthread_mutex_lock(&lock);
        usleep(100);
        ShmPTR->serverflag[index] = NOT_READY;
        ShmPTR->slot[index] = f;
        ShmPTR->serverflag[index] = FILLED;
        pthread_mutex_unlock(&lock);
    }

    pthread_mutex_lock(&lock);
    ShmPTR->arr_count[index]++; // count the number of completed numbers to facotrise
    writeProgress(ShmPTR->arr_count[index], index); // update the progress bar

    if(ShmPTR->arr_count[index] >= 32) // if all facotor is found
    {
      printf("Query %d is done\n", index + 1);
      ShmPTR->queryflag[index] = TAKEN;
      ShmPTR->num_process--; // reducde the amount of numbers processing
      for (int j = 0; j < 9; j++) // reset the progress bar
      {
        ShmPTR->progress[index][j] = '_';
      }
      ShmPTR->progress[index][9] = '|';
      ShmPTR->arr_count[index] = 0;
    }

    pthread_mutex_unlock(&lock);

    return NULL;
}

unsigned long rotation(unsigned long num, int i)
{
  unsigned long rotated_num;
  rotated_num = (num >> i) | (num << 32 - i); // rotate a number right

  return rotated_num;
}

void writeProgress(int num_task, int element)
{
  usleep(10);
  int percentage = num_task * 10 / 32; // calculate percentage
  for (int j = 0; j <= percentage; j++)
  {
    ShmPTR->progress[element][j] = '#'; // update progress bar
  }
}

void testImplement()
{
  pthread_t tid[30]; // create 30 threads for test mode
  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 10; j++)
    {
      usleep(10);
      ShmPTR->counter = i;
      pthread_create(&tid[i * j], NULL, testMode, j);
    }
  }
  ShmPTR->number = TAKEN;
  
  for (int i = 0; i < 300; i++)
  {
    pthread_join(tid[i], NULL);
  }
}

void* testMode(void *arg)
{
  int thread_num = arg;
  int index = ShmPTR->counter;

  for (int i = 0; i < 10; i++)
  {
    pthread_mutex_lock(&lock);
    while(ShmPTR->serverflag[index] != TAKEN);
    srand(time(NULL)); 
    int r = rand() % 91 + 10; // generate random numbers
    float r_time = r / 1000.0;
    sleep(r_time);
    ShmPTR->serverflag[index] = NOT_READY;
    ShmPTR->slot[index] = thread_num * 10 + i;
    ShmPTR->serverflag[index] = FILLED;
    ShmPTR->arr_count[0]++;
    pthread_mutex_unlock(&lock);
  }
}

int main(int argc, char *argv[])
{
  key_t          ShmKEY;
  int            ShmID;
  unsigned long* array[32];
  
  // create a key
  ShmKEY = ftok(".", 'x'); 
  // create an ID
  ShmID = shmget(ShmKEY, sizeof(struct Memory), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | IPC_CREAT);
  if (ShmID < 0) {
    perror("shmget");
      exit(1);
  }
  
  // create a pointer to the structure
  ShmPTR = (struct Memory *) shmat(ShmID, NULL, 0);
  if ((int) ShmPTR == -1) {
      perror("shmat");
      exit(1);
  }

  printf("Shared memory is ready\n");


  for (int i = 0; i < 10; i++) // initiate the progress bar
  {
    for (int j = 0; j < 9; j++)
    {
      ShmPTR->progress[i][j] = malloc(sizeof(char *));
      ShmPTR->progress[i][j] = '_';
    }
    ShmPTR->progress[i][9] = malloc(sizeof(char *));
    ShmPTR->progress[i][9] = '|';
  }

  for (int i = 0; i < 10; i++) // initiate the counter for the number of process done
  {
    ShmPTR->arr_count[i] = 0;
  }

  for (int i = 0; i < 10; i++) // initiate the query flag
  {
    ShmPTR->queryflag[i] = NOT_READY;
  }

  ShmPTR->counter = 0;
  ShmPTR->num_process = 0;
  ShmPTR->quit = 0; // it becomes one when client wants to quit

  pthread_t *tid = malloc(320 * sizeof(pthread_t));

while (1){
  if (ShmPTR->quit == 1) // if client wants to quit
  {
    for (int i = 0; i < 320; i++)
    {
      pthread_join(tid[i], NULL);
    }

    free(tid); 
    printf("   server has informed client data have been taken...\n");
    shmdt((void *) ShmPTR); 
    printf("   server has detached its shared memory...\n");
    shmctl(ShmID, IPC_RMID, NULL);
    printf("   server exits...\n");
    pthread_mutex_destroy(&lock);
    exit(0);
  }

  while (ShmPTR->clientflag != FILLED && ShmPTR->quit == 0)
   sleep(1);
  if(ShmPTR->quit == 1)
  {
    continue;
  }
  
  for (int i = 0; i < 10; i++)
  {
    if(ShmPTR->queryflag[i] == NOT_READY) // seek which slot is avalable
    {
      ShmPTR->counter = i;
      break;
    }
  }

  
  unsigned long num = ShmPTR->number;
  printf("server received %lu in slot[%d]\n", num, ShmPTR->counter + 1);
  
  if (num == 0) // if test mode
  {
    printf("Implement a test mode\n");
    testImplement();
    while (ShmPTR->arr_count[0] < 295)
      usleep(100);
    printf("Test mode is finished\n");
    ShmPTR->arr_count[0] = 0;
    continue;
  }

  ShmPTR->num_process++; // count the slots processing 
  ShmPTR->clientflag = TAKEN;
  ShmPTR->queryflag[ShmPTR->counter] = FILLED;

  if (pthread_mutex_init(&lock, NULL) != 0) { 
      printf("\n mutex init has failed\n");   
      return 1;                               
  }

  for (int i = 0; i < 32; i++)
  {
      ShmPTR->temp = rotation(num, i);
      pthread_create(&tid[ShmPTR->counter * 32 + i], NULL, factorise, ShmPTR);
  }
}
  printf("   server has informed client data have been taken...\n");
  shmdt((void *) ShmPTR);
  printf("   server has detached its shared memory...\n");
  printf("   server exits...\n");
  pthread_mutex_destroy(&lock);
  exit(0);

  return 0;
}