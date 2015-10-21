#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#include <string.h>
#include "Dispatcher.h"
#include "SchedSim.h"

#define DEBUG 0
#define NUMROUNDS 100
#define TIMESLICE 100

struct timedata{
	long avg;
	long min;
	long max;
	int numProc;
	//time_t clockT;
	//struct timeval clockT;
};

typedef struct timedata dataStats;

struct procInf{
	int ppid;
	int priority;
	int fin;
	int io;
	struct timeval clockT;
	dataStats readyTimes;
	dataStats ioTimes;
};

struct qNode{
	int ppid;
	struct qNode* next;
};
typedef struct qNode qNode;

typedef struct procInf procInfo;


				
int firstPID = -100;
int numQueue;


procInfo **procBlock;
int numProc =0;
qNode **queueS;
qNode **queueE;

void addToQueue(procInfo* data){
	qNode *newNode = malloc( sizeof(qNode));
	newNode -> ppid =0;
	newNode -> next = NULL;
	newNode->ppid = data->ppid;
	#if DEBUG
		printf(" data pid is %d, and newnodes pid is %d with priority %d\n", data->ppid, newNode->ppid, data->priority);
	#endif
	if(queueS[data->priority] == NULL){
		queueS[data->priority] = newNode;
		queueE[data->priority] = newNode;
	} else {
		queueE[data->priority]->next = newNode;
		queueE[data->priority] = newNode;
	}

}

//new proc created
void NewProcess(int pid){
	if(firstPID < 0){
		firstPID = pid;
		#if DEBUG
			printf("FIRST PID HAS BEEN SET TO %d\n", pid);
		#endif
	}
	numProc++;
	procBlock = realloc(procBlock, sizeof(procInfo)*numProc );
	procInfo *newProc = malloc( sizeof(procInfo));
	newProc->ppid = pid;
	#if DEBUG
		printf("new process with id %d and inserted pid is %d\n ", pid, newProc->ppid);
	#endif
	if(numQueue == 2){
		newProc->priority = 0;
	} else {
		newProc->priority = numQueue/2;
	}
	newProc->fin = 0;
	//newProc -> readyTimes = malloc( sizeof(dataStats));
	//newProc -> ioTimes = malloc( sizeof(dataStats));
	newProc -> readyTimes.avg = 0;
	newProc -> readyTimes.min =LONG_MAX;
	newProc -> readyTimes.max = 0;
	newProc -> readyTimes.numProc =0;
	newProc -> ioTimes.avg = 0;
	newProc -> ioTimes.min = LONG_MAX;
	newProc -> ioTimes.max = 0;
	newProc -> ioTimes.numProc =0;
	gettimeofday(&newProc->clockT,NULL);
	procBlock[pid-firstPID] = newProc;
	addToQueue(newProc);
	

}

//Getting sent out
void Dispatch(int *pid){
	
	if(numQueue == 2){
		if(queueS[0] == NULL && queueS[1] != NULL){
			queueS[0] = queueS[1];
			queueE[0] = queueE[1];
			queueS[1] = NULL;
			queueE[1] = NULL;
		}
	}
	*pid = 0;
	int i;
	for (i = 0; i < numQueue; i++){
		if(queueS[i] != NULL){
			if( queueS[i] ->ppid > 0 ){
				#if DEBUG
					printf("	-the pid being dispatched it %d from queue # %d\n", queueS[i] ->ppid, i);
				#endif
				*pid = queueS[i]->ppid;
				void *temp = queueS[i];
				if(queueS[i] -> next != NULL){
					queueS[i]= queueS[i]->next;
				}else{
					queueS[i] = NULL;
				}
				//Timing and IO timing information
				struct timeval calcTimes;
				gettimeofday(&calcTimes, NULL);
				//ready timing chunk
				long readyC = ((calcTimes.tv_sec * 1000 + calcTimes.tv_usec/1000 ) -
					 (procBlock[*pid-firstPID]->clockT.tv_sec * 1000 + procBlock[*pid-firstPID]->clockT.tv_usec/1000));
				procBlock[*pid-firstPID] -> readyTimes.avg += readyC;
				procBlock[*pid-firstPID] -> readyTimes.numProc++;
				if(readyC > procBlock[*pid-firstPID]->readyTimes.max){
					procBlock[*pid-firstPID]->readyTimes.max = readyC;
				}
				if (readyC < procBlock[*pid-firstPID]->readyTimes.min){
					procBlock[*pid-firstPID]->readyTimes.min = readyC;
				}
				//IO timing chunk
				if( procBlock[*pid-firstPID] -> io == 1){
					procBlock[*pid-firstPID] -> io = 0;
					procBlock[*pid-firstPID] -> ioTimes.avg += readyC;
					
					if(readyC > procBlock[*pid-firstPID]->ioTimes.max){
					procBlock[*pid-firstPID]->ioTimes.max = readyC;
					}
					if (readyC < procBlock[*pid-firstPID]->ioTimes.min){
						procBlock[*pid-firstPID]->ioTimes.min = readyC;
					}
				}
				free(temp);
				break;
			}
		}
	}
}


//ready to be scheduled
void Ready(int pid, int CPUtimeUsed){
	#if DEBUG
		printf("~process with pid %d is swapping to ready with CPU time used %d ms\n", pid, CPUtimeUsed);
	#endif
	if(CPUtimeUsed >= TIMESLICE){
		if (procBlock[pid-firstPID] -> priority + 1 <= (numQueue -1 )){
			procBlock[pid-firstPID] -> priority++;
			#if DEBUG
				printf("Process with PID %d has been decremented to %d\n", pid, procBlock[pid-firstPID] -> priority);
			#endif
		}
	}
	//Deal with timing information
	gettimeofday(&procBlock[pid-firstPID]->clockT, NULL);
	addToQueue(procBlock[pid-firstPID]);

}

//IO OP
void Waiting(int pid){
	//IO timing information.
	procBlock[pid-firstPID] -> io = 1;
	procBlock[pid-firstPID] -> ioTimes.numProc++;
	if( procBlock[pid-firstPID] -> priority -1 >= 0){
			procBlock[pid - firstPID] -> priority--;
			#if DEBUG
				printf("Process with PID %d has been incremented to %d\n", pid, procBlock[pid-firstPID] -> priority);
			#endif
		}
	#if DEBUG
		printf("process with pid %d is waiting\n", pid);
	#endif

}

//Proc is done don't reschedule
void Terminate (int pid){
	#if DEBUG
		printf("process with pid %d terminated\n", pid);
	#endif
	procBlock[pid-firstPID]->fin = 1;
}

int main(int argc, char *argv[]){
	struct timeval startC;
	gettimeofday(&startC, NULL);
	procBlock = malloc( sizeof(procInfo));
	if(argc > 1 && atoi(argv[1]) >1){
		numQueue = atoi(argv[1]);
		queueS = malloc( sizeof(qNode)* numQueue);
		queueE = malloc( sizeof(qNode) * numQueue);
		Simulate(NUMROUNDS, TIMESLICE);
	}else{
		printf("Didn't supply the number of queues to create aborting or supplied with only one queue \n");
		exit(1);
	}
	struct timeval endC;
	gettimeofday(&endC, NULL);
	//printf("Time program ran for is %f mS\n", difftime(endC, startC));
	printf("Time program ran for is %ld mS\n",((endC.tv_sec * 1000 + endC.tv_usec/1000) - (startC.tv_sec * 1000 + startC.tv_usec/1000)));
	int i;
	for(i =0; i < numProc; i++){
		printf("Process with ID %d was in the ready queue with an average of %f mS, max of %ld mS, and a min of %ld mS\n",
			procBlock[i]->ppid,
			 ((double)(procBlock[i]->readyTimes.avg))/procBlock[i]->readyTimes.numProc,
			  (procBlock[i]->readyTimes.max),
			  (procBlock[i]->readyTimes.min));
		if(procBlock[i] ->ioTimes.numProc > 0){
			printf("	it spent an average of %f ms, max of %ld ms, and min of %ld ms on IO responsiveness\n",
				((double)(procBlock[i]->ioTimes.avg))/procBlock[i]->ioTimes.numProc,
				(procBlock[i]->ioTimes.max),
				(procBlock[i]->ioTimes.min));
		} else {
			printf("	process didn't do any IO waiting\n");
		}
		
	}
	exit(0);
}