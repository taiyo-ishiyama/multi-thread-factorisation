#define  NOT_READY  -1
#define  FILLED     0
#define  TAKEN      1

void *factorise(void *arg);
unsigned long rotation(unsigned long num, int i);
void writeProgress(int num_task, int element);
void* printProgressServer(void* arg);
void testImplement();
void* testMode(void *arg);
void *recvData(void *arg);
void *monitorProgress(void *arg);
void printProgress();


struct Memory {
	int number;
	int slot[10];

	int temp;
	int counter;
	int num_process;
	int quit;

	char clientflag;
	char serverflag[10];

	int arr_count[10];
	char progress[10][10];
	char queryflag[10];
};


