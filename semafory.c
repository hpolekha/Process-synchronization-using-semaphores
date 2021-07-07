/***********************************************************************************************************************
* SOI lab 3 "SEMAFORY"                                                                                                 *
* autor: Halyna Polekha 294866                                                                                         *
*                                                                                                                      *
* -------------------------------------------------Tresc---------------------------------------------------------------*
* Bufor 9-elementowy FIFO. Jest jeden producent i trzech konsumentow (A, B, C). Producent produkuje jeden element,     *
* jezeli jest miejsce w buforze. Element jest usuwany z bufora, jeżeli zostanie przeczytany przez albo obu konsumentow *
* A i B, albo przez obu konsumentow B i C. Konsument A nie może przeczytac elementu, jezeli zostal on juz przez niego  *
* wczesniej przeczytany, albo zostal przeczytany przez konsumenta C i na odwrot.                                       *
***********************************************************************************************************************/


#include <unistd.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <string.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>

#include <time.h>
#include <stdbool.h>


#define MAX 9
#define true 1
#define false 0
#define M 20

typedef struct SemConditions
{
	bool waiting_for_write ;
//	bool waiting_for_write_B ;
	bool waiting_for_read_A ;
	bool waiting_for_read_B ;
	bool waiting_for_read_C ;

}Condition;

 Condition * BindCondition()
{
	static int shmid = 0;

	if(shmid == 0)
		shmid = shmget(IPC_PRIVATE, sizeof(Condition) , SHM_W | SHM_R);

	if(shmid < 0)
	{
		perror("booleans shared memory id");
		abort();
	}

	void * data = shmat(shmid, NULL, 0);

	Condition * lockedBools = (Condition*)data;

	return lockedBools;
}

Condition * InitCondition()
{
	Condition * booleans = BindCondition();

    booleans->waiting_for_write  = false;
	booleans->waiting_for_read_A = false;
	booleans->waiting_for_read_B = false;
	booleans->waiting_for_read_C = false;

	return booleans;
}
typedef enum reader{ NoNe, A, B, C}reader;
typedef struct FIFOQUEUE
{
	short int start, end; //must init with 0
	int size; //must init with 0
	int *prd;
	reader qr;

}Queue;

void printQueue(Queue * q)
{
	printf("  [size fo queue = %d]\n", q->size);
}

void insertElement(Queue * q, char prd)
{
	q->prd[q->end] = prd;
	++(q->end);
	q->end %= MAX;
	q->size++;
}
void setQRead(Queue * q, reader r){
    q->qr = r;
}
reader getQRead(Queue * q){
    return q->qr;
}
char removeElement(Queue * q)
{
	char to_return = q->prd[q->start];
	++(q->start);
	q->start %= MAX;
	q->size--;

	return to_return;
}


Queue * BindQueue()
{

	static int shmid = 0;

	if(shmid == 0)
		shmid = shmget(IPC_PRIVATE, sizeof(Queue) + MAX * sizeof(int) , SHM_W | SHM_R);

	if(shmid <= 0) /* -1 ?*/
	{
		perror("shmid queue");
		abort();
	}

	int * data = shmat(shmid, NULL, 0);

	Queue * queue = (Queue*)data;

	queue->prd = (int*)(data + MAX * sizeof(int));

	return queue;

}

Queue * InitQueue()
{
	Queue * queue = BindQueue();
	queue->start = 0;
	queue->end = 0;
	queue->size = 0;
	queue->qr = NoNe;

	return queue;
}

typedef struct ProjectSemaphores
{

    sem_t write;
	sem_t read_A;
	sem_t read_B;
	sem_t read_C;
	sem_t MUTEX;

}Semaphores;


Semaphores * BindSemaphores()
{
	static int shmid = 0;

	if(shmid == 0)
		shmid = shmget(IPC_PRIVATE, sizeof(struct ProjectSemaphores), SHM_W | SHM_R);

	if(shmid <= 0)
	{
		perror("shmid semaphores");
		abort();
	}
	void * data = shmat(shmid, NULL, 0);
	Semaphores * semCollection = (Semaphores*)data;

	return semCollection;

}



Semaphores * InitSemaphores()
{

	Semaphores * semCollection = BindSemaphores();

	if(sem_init(&semCollection->write, 1, 0))
		perror("sem_init fail");
	if(sem_init(&semCollection->read_A, 1, 0))
		perror("sem_init fail");
	if(sem_init(&semCollection->read_B, 1, 0))
		perror("sem_init fail");
    if(sem_init(&semCollection->read_C, 1, 0))
		perror("sem_init fail");
	if(sem_init(&semCollection->MUTEX, 1, 1))
		perror("sem_init fail");

	return semCollection;
}


void wait_sem(sem_t * s)
{
	if(sem_wait(s))
		perror("wait_sem on semaphore");
}

void signal_sem(sem_t * s)
{
	if(sem_post(s))
		perror("up on semaphore");
}

void CreateSubProc(void (* function)())
{
	if(fork() == 0)
	{
        printf(" PROCESS CREATED\n");
		function();
		exit(0);
	}
}
unsigned int IndepRand()						/*Potrzebne sa wartosci losowe*/
{
	FILE * F = fopen("/dev/urandom", "r");
	if (!F)
	{
		printf("Cannot open urandom...\n");
		abort();
	}
	unsigned int Ret;
	unsigned int X = fread((char *)&Ret, 1, sizeof(unsigned int), F);
	fclose(F);

	return Ret;
}

void Producer()
{
    printf("~~ start zycia P\n");
	Queue * queue = BindQueue();
	Semaphores * semaphores = BindSemaphores();
	Condition * booleans = BindCondition();

int tasks = 0;
while(tasks<M)
	{
        usleep((IndepRand() % 500000));
		wait_sem(&semaphores->MUTEX);
        printf("P zachodzi w SK\n");
;		if (queue->size == MAX)

		{
			booleans->waiting_for_write = true;
			printf("  [pusta kolejka]\n");
            printf("interrupted P, wychodzi z SK \n");
			signal_sem(&semaphores->MUTEX);
			wait_sem(&semaphores->write);
			printf("continue P, wraca do SK\n");
			wait_sem(&semaphores->MUTEX);

		}

		booleans->waiting_for_write = false;

		printf("  [Producent produkuje]\n");
		char prd = 'P';

		insertElement(queue, prd);
		tasks++;
		printf("  [falg = %d] \n", queue->qr);
		printQueue(queue);
		if (booleans->waiting_for_read_A && queue->size > 0)
            signal_sem(&semaphores->read_A);
		else if (booleans->waiting_for_read_B && queue->size > 0)
			signal_sem(&semaphores->read_B);
		else if (booleans->waiting_for_read_C && queue->size > 0)
			signal_sem(&semaphores->read_C);
        tasks++;
        printf("P wychodzi z SK\n");
        signal_sem(&semaphores->MUTEX);

	}
	printf("~~~~~~~~~~ P - KONIEC DZIALANIA ~~~~~~~~~\n");

}


void Consumer_A()
{
    printf("~~ start zycia A\n");
	Queue * queue = BindQueue();
	Semaphores * semaphores = BindSemaphores();
	Condition * booleans = BindCondition();
    int tasks = 0;
	while(tasks<M)
	{
	    usleep((IndepRand() % 500000));
		wait_sem(&semaphores->MUTEX);
        printf("A zacodzi w SK\n");

		if (queue->size == 0)

		{
			booleans->waiting_for_read_A = true;
			printf("  [pusta kolejka]\n");
            printf("interrupted A, wychodzi z SK\n");
			signal_sem(&semaphores->MUTEX);
			wait_sem(&semaphores->read_A);
			printf("continue A, wraca do SK\n");
			wait_sem(&semaphores->MUTEX);

		}

		booleans->waiting_for_read_A = false;
        printf("  [falg = %d] \n", queue->qr);
        switch(queue->qr)
        {
        case A: break;
        case C: break;
        case B: printf("  [A konsumuje]\n");
                removeElement(queue);
                queue->qr = NoNe;
                tasks++;
                printQueue(queue);
                break;

        default: queue->qr = A;
                 printf("  [czyta A]\n");
        }



		if (booleans->waiting_for_write && queue->size < MAX )
			signal_sem(&semaphores->write);
		else if (booleans->waiting_for_read_B && queue->size > 0)
			signal_sem(&semaphores->read_B);
        else if (booleans->waiting_for_read_C && queue->size > 0)
			signal_sem(&semaphores->read_B);
        tasks++;
        printf("A wychodzi z SK\n");
        signal_sem(&semaphores->MUTEX);

	}
    printf("~~~~~~~~~~ A - KONIEC DZIALANIA ~~~~~~~~~\n");
}
void Consumer_B()
{

    printf("~~ start zycia B\n");
	Queue * queue = BindQueue();
	Semaphores * semaphores = BindSemaphores();
	Condition * booleans = BindCondition();
    int tasks = 0;
	while(tasks<20)
	{
	    usleep((IndepRand() % 500000));
		wait_sem(&semaphores->MUTEX);
        printf("B zachodzi w SK\n");
		if (queue->size == 0)
		{
			booleans->waiting_for_read_B = true;
			printf("  [pusta kolejka]\n");
            printf("interrupted B, wychodzi z SK\n");
			signal_sem(&semaphores->MUTEX);
			wait_sem(&semaphores->read_B);
            printf("continue B, wraca do SK\n");
            wait_sem(&semaphores->MUTEX);
		}

		booleans->waiting_for_read_B = false;
        printf("  [falg = %d] \n", queue->qr);
        switch(queue->qr)
        {
        case A:
        case C: printf("  [B konsumuje]\n");
                removeElement(queue);
                queue->qr = NoNe;
                tasks++;
                printQueue(queue);
                 break;
        case B:  break;
        default: queue->qr = B;
                 printf("  [czyta B]\n");
        }

		if (booleans->waiting_for_write && queue->size < MAX )
			signal_sem(&semaphores->write);
		else if (booleans->waiting_for_read_A && queue->size > 0)
			signal_sem(&semaphores->read_A);
        else if (booleans->waiting_for_read_C && queue->size > 0)
			signal_sem(&semaphores->read_C);
        tasks++;
        printf("B wychodzi z SK\n");
        signal_sem(&semaphores->MUTEX);

    }
    printf("~~~~~~~~~~ B - KONIEC DZIALANIA ~~~~~~~~~\n");
}


void Consumer_C()
{
    printf("~~ start zycia C\n");
	Queue * queue = BindQueue();
	Semaphores * semaphores = BindSemaphores();
	Condition * booleans = BindCondition();
    int tasks = 0;
	while(tasks<M)
	{
        usleep((IndepRand() % 500000));
        wait_sem(&semaphores->MUTEX);
        printf("C zachodzi w SK\n");
		if (queue->size == 0)
		{
			booleans->waiting_for_read_C = true;
			printf("  [pusta kolejka]\n");
            printf("interrupted C, wychodzi z SK\n");
			signal_sem(&semaphores->MUTEX);
			wait_sem(&semaphores->read_C);
			printf("continue C, wraca do SK\n");
			wait_sem(&semaphores->MUTEX);

		}

		booleans->waiting_for_read_C = false;
        printf("  [falg = %d ]\n", queue->qr);
        switch(queue->qr)
        {
        case A: break;
        case C: break;
        case B: printf("  [C konsumuje]\n");
                removeElement(queue);
                queue->qr = NoNe;
                tasks++;
                printQueue(queue);
                break;
        default: queue->qr = C;
                 printf("  [czyta C]\n");

        }

		if (booleans->waiting_for_write && queue->size < MAX)
			signal_sem(&semaphores->write);
		else if (booleans->waiting_for_read_A && queue->size > 0)
			signal_sem(&semaphores->read_A);
		else if (booleans->waiting_for_read_B && queue->size > 0)
			signal_sem(&semaphores->read_B);
        tasks++;
        printf("C wychodzi z SK\n");
        signal_sem(&semaphores->MUTEX);

	}
    printf("~~~~~~~~~~ C - KONIEC DZIALANIA ~~~~~~~~~\n");
}


int main(int argc, char **argv)
{
	InitQueue();
	InitSemaphores();
	InitCondition();

    CreateSubProc(&Producer);
	CreateSubProc(&Consumer_A);
	CreateSubProc(&Consumer_B);


    CreateSubProc(&Consumer_C);

	//while (1) {
    //    sleep(1);
    //}
    printf("-----------------------------------------END-------------------------------------------------------------\n");

	return 0;

}
