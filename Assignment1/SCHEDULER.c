/*
David Shagam
Operating Systems 460
Assignment 1: Scheduler algorithms
run and test multiple scheduler algorithms comparing performance under varying time slices

*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#include <string.h>
#include "Dispatcher.h"
#include "SchedSim.h"

//Constants
#define DEBUG 0
#define NUMROUNDS 100
//in ms
#define TIMESLICE 100

//Data for the times of each process.
struct timedata{
	long avg;
	long min;
	long max;
	int numProc;
};

typedef struct timedata dataStats;

//Info on each process for the process control block
struct procInf{
	int ppid;
	int priority;
	int fin;
	int io;
	struct timeval clockT;
	struct timeval dispTime;
	dataStats readyTimes;
	dataStats ioTimes;
};

//Node structure for a linked list. Done as a queue.
struct qNode{
	int ppid;
	struct qNode* next;
};
typedef struct qNode qNode;

typedef struct procInf procInfo;


//Globals for the number of queues to make and the first PID recieved				
//firstPID used for indexing into PCB
int firstPID = -100;
int numQueue;

int overhead = 0;

//Array of pointers to process control blocks
procInfo **procBlock;
//Total number of processes Used for printing through the entirety of the process controll block
//and allocating space
int numProc =0;
//start and end of the array of queues
qNode **queueS;
qNode **queueE;

//Helper function to enqueue processes
//Takes a pointer to a process control block
void addToQueue(procInfo* data){
	//Creates a new linked list node
	qNode *newNode = malloc( sizeof(qNode));
	newNode -> ppid =0;
	newNode -> next = NULL;
	newNode->ppid = data->ppid;
	#if DEBUG
		printf(" data pid is %d, and newnodes pid is %d with priority %d\n", data->ppid, newNode->ppid, data->priority);
	#endif
		//If the start of the list is null for the desired priority then node becomes the start and end.
		//else tack it on to the end.
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
	//instantiates the base PID recieved. This is used for "hashing" into the PCB
	if(firstPID < 0){
		firstPID = pid;
		#if DEBUG
			printf("FIRST PID HAS BEEN SET TO %d\n", pid);
		#endif
	}
	numProc++;
	//Increase size of the PCB
	procBlock = realloc(procBlock, sizeof(procInfo)*numProc );
	//create a new PCB entry
	procInfo *newProc = malloc( sizeof(procInfo));
	newProc->ppid = pid;
	#if DEBUG
		printf("new process with id %d and inserted pid is %d\n ", pid, newProc->ppid);
	#endif
	//Special case for ULE algorithm EG 2 queues.
	//Else just put it in the middle queue
	if(numQueue == 2){
		newProc->priority = 0;
	} else {
		newProc->priority = (numQueue-1)/2;
	}
	//Init data to default values
	newProc->fin = 0;
	newProc -> readyTimes.avg = 0;
	newProc -> readyTimes.min =LONG_MAX;
	newProc -> readyTimes.max = 0;
	newProc -> readyTimes.numProc =0;
	newProc -> ioTimes.avg = 0;
	newProc -> ioTimes.min = LONG_MAX;
	newProc -> ioTimes.max = 0;
	newProc -> ioTimes.numProc =0;
	gettimeofday(&newProc->clockT,NULL);
	//assign data of new PCB entry to the PCB array
	procBlock[pid-firstPID] = newProc;
	addToQueue(newProc);
	

}

//fills a provided int address with the PID of the highest priority process
//return of 0 means no processes found it should hopefully never do this.
void Dispatch(int *pid){
	
	//Special case for ULE in which we swap next queue to the current queue and blank out next.
	if(numQueue == 2){
		if(queueS[0] == NULL && queueS[1] != NULL){
			queueS[0] = queueS[1];
			queueE[0] = queueE[1];
			queueS[1] = NULL;
			queueE[1] = NULL;
		}
	}
	//Just incase
	*pid = 0;

	//Find PID to send out
	int i;
	for (i = 0; i < numQueue; i++){

		//Make sure theres stuff in this queue
		if(queueS[i] != NULL){

			//make sure it's not a glitched case
			if( queueS[i] ->ppid > 0 ){
				#if DEBUG
					printf("	-the pid being dispatched it %d from queue # %d\n", queueS[i] ->ppid, i);
				#endif

				//grab the value
				*pid = queueS[i]->ppid;

				//maintain a pointer for mem management
				void *temp = queueS[i];

				//Manage the queue
				if(queueS[i] -> next != NULL){
					queueS[i]= queueS[i]->next;
				}else{
					queueS[i] = NULL;
				}

				//Timing and IO timing information
				struct timeval calcTimes;
				gettimeofday(&calcTimes, NULL);

				//ready timing chunk
				//Messy looking math that gets the difference in time in ms for stats.
				long readyC = ((calcTimes.tv_sec * 1000 + calcTimes.tv_usec/1000 ) -
					 (procBlock[*pid-firstPID]->clockT.tv_sec * 1000 + procBlock[*pid-firstPID]->clockT.tv_usec/1000));
				procBlock[*pid-firstPID] -> readyTimes.avg += readyC;
				procBlock[*pid-firstPID] -> readyTimes.numProc++;
				//check if it's a new max
				if(readyC > procBlock[*pid-firstPID]->readyTimes.max){
					procBlock[*pid-firstPID]->readyTimes.max = readyC;
				}
				//check if it's a new min
				if (readyC < procBlock[*pid-firstPID]->readyTimes.min){
					procBlock[*pid-firstPID]->readyTimes.min = readyC;
				}
				//IO timing chunk
				//Make sure we actually waited on an IO to take relevant stats
				if( procBlock[*pid-firstPID] -> io == 1){
					//we completed the IO, waiting on IO stats false
					procBlock[*pid-firstPID] -> io = 0;
					procBlock[*pid-firstPID] -> ioTimes.avg += readyC;
					
					//check for new max
					if(readyC > procBlock[*pid-firstPID]->ioTimes.max){
					procBlock[*pid-firstPID]->ioTimes.max = readyC;
					}
					//check for new min
					if (readyC < procBlock[*pid-firstPID]->ioTimes.min){
						procBlock[*pid-firstPID]->ioTimes.min = readyC;
					}
				}
				procBlock[*pid-firstPID]->dispTime = calcTimes;
				//free the old node that is  unneeded
				free(temp);
				//exit loop becaues we found the thing.
				break;
			}
		}
	}
}


//ready to be scheduled
//takes the pid of a ready process and the ammount of ms time used.
void Ready(int pid, int CPUtimeUsed){
	overhead+= CPUtimeUsed;
	#if DEBUG
		printf("~process with pid %d is swapping to ready with CPU time used %d ms\n", pid, CPUtimeUsed);
	#endif
	//Make the thing less important it used all of it's time.
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

//waiting on IO op
//takes the pid of the process waiting.
void Waiting(int pid){
	//IO timing information.
	procBlock[pid-firstPID] -> io = 1;
	procBlock[pid-firstPID] -> ioTimes.numProc++;
	//Make the proces more important.
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
//takes the pid of the terminating process
void Terminate (int pid){
	#if DEBUG
		printf("process with pid %d terminated\n", pid);
	#endif
	procBlock[pid-firstPID]->fin = 1;
	//used for calculating overhead
	struct timeval calcTimes;
	gettimeofday(&calcTimes, NULL);
	int  tempC = ((calcTimes.tv_sec * 1000 + calcTimes.tv_usec/1000 ) -
		(procBlock[pid-firstPID]->dispTime.tv_sec * 1000 + procBlock[pid-firstPID]->dispTime.tv_usec/1000));
	overhead += tempC;
}

//Takes one argument the number of queues to use.
//case 1) 2 queues = ULE
//case 2) #>2 queues = 4BSD
int main(int argc, char *argv[]){
	//gets the start time
	struct timeval startC;
	gettimeofday(&startC, NULL);

	//creates intial PCB array
	procBlock = malloc( sizeof(procInfo));
	//checks for args
	if(argc > 1 && atoi(argv[1]) >1){
		numQueue = atoi(argv[1]);
		queueS = malloc( sizeof(qNode)* numQueue);
		queueE = malloc( sizeof(qNode) * numQueue);
		//runs simulate
		Simulate(NUMROUNDS, TIMESLICE);
	}else{
		printf("Didn't supply the number of queues to create aborting or supplied with only one queue \n");
		exit(1);
	}
	//grabs the ending time
	struct timeval endC;
	gettimeofday(&endC, NULL);

	//data prints
	printf("Time program ran for is %ld ms\n",((endC.tv_sec * 1000 + endC.tv_usec/1000) - (startC.tv_sec * 1000 + startC.tv_usec/1000)));
	int i;
	for(i =0; i < numProc; i++){
		if( procBlock[i]->readyTimes.numProc>0){
			printf("	Process with ID %d was in the ready queue with an average of %f ms, max of %ld ms, and a min of %ld ms\n",
				procBlock[i]->ppid,
				 ((double)(procBlock[i]->readyTimes.avg))/procBlock[i]->readyTimes.numProc,
				  (procBlock[i]->readyTimes.max),
				  (procBlock[i]->readyTimes.min));
			if(procBlock[i] ->ioTimes.numProc > 0){
				printf("		it spent an average of %f ms, max of %ld ms, and min of %ld ms on IO responsiveness\n",
					((double)(procBlock[i]->ioTimes.avg))/procBlock[i]->ioTimes.numProc,
					(procBlock[i]->ioTimes.max),
					(procBlock[i]->ioTimes.min));
			} else {
				printf("		process didn't do any IO waiting\n");
			}
		} else {
			printf("	Process with PID %d was never dispatched\n", procBlock[i]->ppid);
		}
		
	}
	printf("Time program spent on scheduler overheads is %ld ms\n",  ((endC.tv_sec * 1000 + endC.tv_usec/1000) - (startC.tv_sec * 1000 + startC.tv_usec/1000)) -overhead);
	exit(0);
}