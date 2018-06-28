#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
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

char shmName[128];
char semName[128];
char keyword[128];

int main(int argc, char *argv[])
{ 
	strcpy(shmName, argv[1]);
	strcpy(keyword, argv[2]);
	strcpy(semName, argv[3]);
	int shm_fd;
	void *ptr;

	// create the shared memory segment
	shm_fd = shm_open(shmName,O_RDWR, 0666);

	// size of the shared memory segment 
	ftruncate(shm_fd,sizeof(memoryStructure));

	// map the shared memory segment 
	ptr = mmap(0,sizeof(memoryStructure), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

	if (ptr == MAP_FAILED) {
		printf("Map failed\n");
		return -1;
	}

	memoryStructure *sp;

	sp = (memoryStructure *)ptr;
	
	//declare semaphores

	//semaphore for the state_queue
	sem_t * sqSem ;
	
	//three kind of semaphores for the result queues
	sem_t * rqMutexSems[10];
	sem_t * rqEmptySems[10];
	sem_t * rqFullSems[10];
	
	//three semaphore for request queue	
	sem_t * reqFull;
	sem_t * reqEmpty;
	sem_t * reqMut;
	

	//create semaphore for queue_state
	char sqSemName[128];
	strcpy(sqSemName,semName);
	strcat(sqSemName,"_sq");
	sqSem = sem_open(sqSemName,O_EXCL, 0644, 1);

	//create semophores for result queues
	char  rqMutexNames[10][128];
	char  rqFullNames[10][128];
	char  rqEmptyNames[10][128];

	for(int i=0; i<10 ; i++)
	{	
	
		//mutex semaphores
		sprintf(rqMutexNames[i],"%s%s%d",semName,"_rqMutex_",i);
		rqMutexSems[i] = sem_open(rqMutexNames[i], O_EXCL, 0644, 1);

		//empty semaphores
		sprintf(rqEmptyNames[i],"%s%s%d",semName,"_rqEmpty_",i);
		rqEmptySems[i] = sem_open(rqEmptyNames[i],  O_EXCL, 0644, 100);

		//full semaphores
		sprintf(rqFullNames[i],"%s%s%d",semName,"_rqFull_",i);
		rqFullSems[i] = sem_open(rqFullNames[i],  O_EXCL, 0644, 0) ;

	}

	//create semaphore for request queue		
	char  reqMutn[128] ;
	char  reqEmptyn[128] ;
	char  reqFulln[128] ;
	
	sprintf(reqMutn,"%s%s",semName,"_reqMutex");
	sprintf(reqEmptyn,"%s%s",semName,"_reqEmpty");
	sprintf(reqFulln,"%s%s",semName,"_reqFull");
	
	reqMut = sem_open(reqMutn, O_EXCL, 0644, 1);
	reqFull = sem_open(reqFulln,  O_EXCL, 0644, 0);
	reqEmpty = sem_open(reqEmptyn,  O_EXCL, 0644, 10);			




	int client_id = -1;

	//check for the unused one in state queue
	sem_wait(sqSem);
	
	for(int i=0 ; i<10 ; i++)
	{
		if(sp->state_queue[i]==0)
		{
		
			sp->state_queue[i] = 1;
			client_id = i;
		
			break;
		}
		
	}

	sem_post(sqSem);
	
	// if there are ten clients working
	if(client_id==-1)
	{
		printf("%s\n","too many clients" );
		return -1;
	}

	//create new request
	request rq;
	rq.client_id= client_id;
	
	
	strcpy(rq.keyword,keyword);
	

	//push request to request queue
	sem_wait(reqEmpty);
	sem_wait(reqMut);
    
	sp->request_queue[sp->inreq] = rq;
	sp->inreq = (sp->inreq + 1) % 10 ;
	
	sem_post(reqMut);
	sem_post(reqFull);
	
	
	//initialize output
	int output = 0;

	
	// get output from related result queue and print it
	while(output!=-1)
	{	
	
		sem_wait(rqFullSems[client_id]);
		sem_wait(rqMutexSems[client_id]);
		
		output = sp->result_queues[client_id][sp->outrq[client_id]];
		
		if(output!=-1)
        	printf("%d %s\n", output,keyword);
       
		sp->outrq[client_id] = (sp->outrq[client_id] + 1) % 100 ;

	   
		
		sem_post(rqMutexSems[client_id]);
		sem_post(rqEmptySems[client_id]);
	       

	}

	sp->inrq[client_id]= 0; 
	sp->outrq[client_id] = 0;

	sem_wait(sqSem);
	sp->state_queue[client_id] = 0;
	sem_post(sqSem);
	
	return 0;

}
