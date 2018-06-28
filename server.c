#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

//global struct request
typedef struct requests
	{
		int client_id;
		char keyword[128];
	}request;

//global struct memoryStructure
typedef struct memory{

	//state queue
	int state_queue[10];

	//result queues
	int result_queues[10][100];
	int inrq[10];//next free position
	int outrq[10];//first full position
	
	//request queue
	request request_queue[10];
	int inreq;//next free position
	int outreq;//first full position
	
	
}memoryStructure;


//declare semaphores as global

//semaphore for the state_queue
sem_t * sqSem ;

//three semaphores for the result queues
sem_t * rqMutexSems[10];
sem_t * rqEmptySems[10];
sem_t * rqFullSems[10];

//three semaphores for request queue	
sem_t * reqFull;
sem_t * reqEmpty;
sem_t * reqMut;

//global variables
char  shmName[128];
char  semName[128];
char  inputFileName[128];

void * newthread(void* req);

void sigintHandler(int sig_num);

memoryStructure *sp;

//names for semaphores of result queues
char  rqMutexNames[10][128];
char  rqFullNames[10][128];
char  rqEmptyNames[10][128];

//names for semaphores of request queue
char reqMutn[128] ;
char reqEmptyn[128] ;
char reqFulln[128] ;

// names- of semaphore for state queue
char  sqSemName[128];

int main(int argc, char *argv[])
{ 
	signal(SIGINT, sigintHandler);

	strcpy(shmName,argv[1]);
	strcpy(inputFileName,argv[2]);
	strcpy(semName,argv[3]);

	
	int shm_fd;
	void *ptr;

	/* create the shared memory segment */
	shm_fd = shm_open(shmName, O_CREAT | O_RDWR, 0666);

	/* configure the size of the shared memory segment */
	ftruncate(shm_fd,sizeof(memoryStructure));

	/* now map the shared memory segment in the address space of the process */
	ptr = mmap(0,sizeof(memoryStructure), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

	if (ptr == MAP_FAILED) 
	{
		printf("Map failed\n");
		return -1;
	}

	
	//put struct to shared memory address
	sp = (memoryStructure *)ptr;
    
	
	//initialize state_queue to zero
	for(int i=0; i<10; i++)
	{
		sp->state_queue[i]=0;
	}

	//initiliaze in and out pointer of buffers
	sp->inreq = 0;
	sp->outreq = 0 ;

	for(int i=0 ; i<10; i++)
	{
		sp->inrq[i] = 0;
		sp->outrq[i] = 0;
	}
	
	
	//create semaphore for queue_state
	strcpy(sqSemName,semName);
	strcat(sqSemName,"_sq");
	sqSem = sem_open(sqSemName, O_CREAT | O_EXCL, 0644, 1);

	//initiliaze semophores for result queues
	for(int i=0; i<10 ; i++)
	{	
	
		//mutex semaphores
		sprintf(rqMutexNames[i],"%s%s%d",semName,"_rqMutex_",i);
		rqMutexSems[i] = sem_open(rqMutexNames[i], O_CREAT | O_EXCL, 0644, 1);

		//empty semaphores
		sprintf(rqEmptyNames[i],"%s%s%d",semName,"_rqEmpty_",i);
		rqEmptySems[i] = sem_open(rqEmptyNames[i], O_CREAT | O_EXCL, 0644, 100);

		//full semaphores
		sprintf(rqFullNames[i],"%s%s%d",semName,"_rqFull_",i);
		rqFullSems[i] = sem_open(rqFullNames[i], O_CREAT | O_EXCL, 0644, 0) ;

	}

	
	//initiliaze semaphore for request queue		
	sprintf(reqMutn,"%s%s",semName,"_reqMutex");
	sprintf(reqEmptyn,"%s%s",semName,"_reqEmpty");
	sprintf(reqFulln,"%s%s",semName,"_reqFull");
	
	reqMut = sem_open(reqMutn, O_CREAT | O_EXCL, 0644, 1);
	reqFull = sem_open(reqFulln, O_CREAT | O_EXCL, 0644, 0);
	reqEmpty = sem_open(reqEmptyn, O_CREAT | O_EXCL, 0644, 10);			
	
	request rq ;
	

	//work of server
	while(1)
	{	
		sem_wait(reqFull);
		sem_wait(reqMut);
		
		rq = sp->request_queue[sp->outreq];
		sp->outreq = (sp->outreq + 1) % 10 ;
		request *rqSend = malloc(sizeof(request));
		rqSend->client_id = rq.client_id;
		strcpy(rqSend->keyword,rq.keyword);
		pthread_t thread;
 
		pthread_create(&thread,NULL, newthread,(void *) rqSend);

		sem_post(reqMut);
		sem_post(reqEmpty);
		
	}
}


void* newthread(void* req)
{
	char line[1024];
	char word[128];
	request *rq = (request *)(req);
	strcpy(word,rq->keyword);
	
	
	int lineNo = 1;
	int val;

	FILE* file = fopen(inputFileName, "r"); 
	
	while (fgets(line, sizeof(line), file))
	{
       
        if(strstr(line,word))
        {	
        	
        	sem_wait(rqEmptySems[rq->client_id]);
        	sem_wait(rqMutexSems[rq->client_id]);
        	
        	sp->result_queues[rq->client_id][sp->inrq[rq->client_id]] = lineNo;
        	sp->inrq[rq->client_id] = (sp->inrq[rq->client_id] + 1) % 100 ;
        	
            sem_post(rqMutexSems[rq->client_id]);
         	sem_post(rqFullSems[rq->client_id]);
        	
        }

        lineNo++;
   
    }
   	
	sem_wait(rqEmptySems[rq->client_id]);
	sem_wait(rqMutexSems[rq->client_id]);

	sp->result_queues[rq->client_id][sp->inrq[rq->client_id]] = -1;
	//sp->inrq[rq->client_id] = (sp->inrq[rq->client_id] + 1) % 100 ;

	sem_post(rqMutexSems[rq->client_id]);
	sem_post(rqFullSems[rq->client_id]);
	
	
	
	//fclose(file);
	
	pthread_exit(0); 

}

	


void sigintHandler(int sig_num)
{
    
	//printf("interrupt catched");
	shm_unlink(shmName);
	sem_unlink(sqSemName);
	sem_unlink(reqMutn);
	sem_unlink(reqEmptyn);
	sem_unlink(reqFulln);

	for(int i =0 ; i<10 ; i++)
	{
		sem_unlink(rqMutexNames[i]);
		sem_unlink(rqFullNames[i]);
		sem_unlink(rqEmptyNames[i]);	
	}	

	exit(sig_num);
}
