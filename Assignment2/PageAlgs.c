//David Shagam
//Program 2 Page algorithms
//Operating Systems 460

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "Assg2.h"
#include "MemSim.h"

#define MAXPROC 16 // num
#define PAGESIZE 4096 //bytes THIS IS ASSUMING MEBI
#define MEMAVAIL 8388608 //bytes THIS IS ASSUMING MEBI 
#define ADDRSIZE 32 //bits
#define DEBUG 0
#define ADDRMASK 0xfffff000
#define AGE 1000

struct pageData{
	int pid;
	int addr; // first 20 bits are addr, last 12 are offset
	uint Acc;
	int Dirt;
};

typedef struct pageData page;

int whichMode; //1 = LRU, 2 = LFU
int numRequests;

int numReadFault;
int numRWFault;
int numProc;
int curPages;
int numPages;
page* pageTable;

int lastReplaced;

//Ages all processes
void doAge(){
	int i;
	for(i =0; i < numPages; i++){
		pageTable[i].Acc = pageTable[i].Acc / 2;
	}
}

//checks if a process has anythign in hte page table
int ContainsPID(int pid){
	int i;
	for(i = 0; i<numPages; i++){
		if(pageTable[i].pid == pid){
			return i;
		}
	}
	return -1;
}

//Sets the values for an entry to values
void setEntry(int index, int pid, int address, int write){
	pageTable[index].pid = pid;
	pageTable[index].addr = address;
	pageTable[index].Acc = 1;
	if(write == 1){
		pageTable[index].Dirt = 1;
	} else {
		pageTable[index].Dirt = 0;
	}
}

//Finds hte least frequently used item to replace
//does book keeping and replacing entries
void LFU(int pid, int address, int write){
	numRequests++;
	int repInd = -1;
	int i;
	uint smallest = INT_MAX;
	for(i = 0; i < numPages; i++){
		if(pageTable[i].Acc < smallest){
			repInd = i;
			smallest = pageTable[i].Acc;
		}
	}
	//replace here;
	if(pageTable[repInd].Dirt ==1){
		numRWFault++;
	} else {
		numReadFault++;
	}
	setEntry(repInd, pid, address, write);
	pageTable[repInd].Acc = 0;
	#if DEBUG
		printf("~~ page table full and not already in replaced\n");
	#endif
	
}

//Finds the Least recently used thing to replace
//sets values and replaces an entry in the page table
void LRU(int pid, int address, int write){
	//replacement 5 is off the charts, priroity is 1>2>3>4
	//which index we are replacing
	int repInd = -1;
	//which importance we are replacing with
	int repType = 5;
	int j;
	for (j = 0; j < numPages; j++){
		int efInd = (j+ lastReplaced) % numPages;// effective index
		//Most important thing replace it now
		if( (pageTable[efInd].Acc == 0 && pageTable[efInd].Dirt ==0)
		 && repType > 1 ){
			repType = 1;
			repInd = efInd;
		 	break;
		} else if( (pageTable[efInd].Acc == 0 && pageTable[efInd].Dirt ==1)
		 && repType > 2){
		 	//Not as important keep looking
			repType = 2;
			repInd = efInd;
		} else if ( (pageTable[efInd].Acc == 1 && pageTable[efInd].Dirt ==0)
		 && repType >3){
		 	//Even less importnat keep looking
			repType = 3;
			repInd = efInd;

		} else if ( (pageTable[efInd].Acc == 1 && pageTable[efInd].Dirt ==1)
		 && repType > 4){
		 	//least important to replace with keep looking
			repType = 4;
			repInd = efInd;
		}
	}
	//Replace the table entry;
	if(repInd != -1){
		if(pageTable[repInd].Dirt ==1){
			numRWFault++;
		} else {
			numReadFault++;
		}
		setEntry(repInd, pid, address, write);
	} else{
		//horrible error
		exit(1);
	}
	//Blank the access bit
	for(j = 0; j < numPages; j++){
		int efInd = (j+lastReplaced)% numPages;
		if( efInd == repInd){
			break;
		}
		if(efInd != lastReplaced){
			pageTable[efInd].Acc = 0;
		}
	}
	lastReplaced = repInd;
	#if DEBUG
		printf("~~ page table full and not already in replaced\n");
	#endif

}

//Takes a PID, address, and if it's read or write to repalce figure out
//If we replace things or add thigns or if it exists
//returns 1 for added/accessed or 0 for rejecting the process
int Find(int pid, int address, int write){
	//Find if the PID has ANYTHING in here.
	int startAddr = ContainsPID(pid);
	//If full
	if(numProc == MAXPROC){
		//And we don't already have the PID don't let process start
		if(startAddr == -1){
			#if DEBUG
				printf("~~ can't add another process\n");
			#endif
			return 0; //Full on proc and can't do this
		}
	}
	// We aren't full and we don't have it add a process
	if( startAddr == -1){
		numProc++;
	}

	//check for it's currente existance.
	int i;
	int ret = 0;
	for(i = startAddr; i < numPages; i++){
		if(pageTable[i].pid == pid && (pageTable[i].addr & ADDRMASK) == (address & ADDRMASK)){
			//LRU
			if(whichMode == 1){
			pageTable[i].Acc = 1;
			//LFU
			} else if(whichMode == 2){
				//Add frequency of use
				pageTable[i].Acc = pageTable[i].Acc >>1;
				pageTable[i].Acc += 0x80000000;
			}
			//Set dirty bit
			if(write == 1){
				pageTable[i].Dirt = 1;
			}
			#if DEBUG
				printf("~~ Page already exists and we toched it\n");
			#endif
			ret = 1; // we have the page and touched it.
			break;
		}
	}
	//Don't already have it
	if(ret != 1){
		if(curPages >= numPages){
			//It's full
			//Use LRU
			if(whichMode == 1){
				LRU(pid, address, write);
			//use LFU
			} else if(whichMode == 2){
				LFU(pid, address, write);
			}

		} else {
			//find a space
			curPages++;
			int j;
			for(j = 0; j  < numPages; j++ ){
				int efInd = (j + lastReplaced) % numPages;
				if(pageTable[efInd].pid == -1){
					//Add the element to first free space we can find.
					if(pageTable[efInd].Dirt == 1){
						numRWFault++;
					}else {
						numReadFault++;
					}
					setEntry(efInd, pid, address, write);
					if(whichMode == 2){
						pageTable[efInd].Acc = 0;
					}
					break;
				}
			}
			#if DEBUG
				printf("~~ Page table had room and we didn't already contain it\n");
			#endif
		}
	}
	return 1;
	
}

//Called when soemthing attempts to acces 
//returns 0 for reject process, 1 for accept processes/access
int Access(int pid, int address, int write){
	//Access request increment for LFU
	if(whichMode == 2){
		numRequests++;
	}
	#if DEBUG
		printf("Access called with pid %d, addr %d, write %d\n",pid, address, write );
	#endif
	//Find if valid or invalid
	int retVal = Find(pid, address, write);
	//Age the processes
	if(numRequests >= AGE){
		doAge();
		numRequests = 0;
	}
	#if DEBUG
		printf("~~!returning value %d\n", cases);
	#endif
	return retVal;
}

//Called when a process terminates
//Frees up pages used by said process
void Terminate(int givenpid){
	#if DEBUG
		printf("--terminated called with given pid %d\n",givenpid);
	#endif
	//Book keeping number of processes
	numProc--;
	//Free the pages
	int i;
	for(i = 0; i< numPages; i++){
		if(pageTable[i].pid == givenpid){
			pageTable[i].pid = -1;
			curPages--;
		}
	}
}

//Main takes 1 argumnet being -lfu or -lru which is the replacement algroithm to run
//outputs text with stats about how many page faults happened
int main(int argc, char* argv[]){
	//Setup argument info
	if(argc < 2){
		printf("Error please use -lfu or -lru flags \n");
		exit(1);
	} else {
		if( strcmp(argv[1], "-lfu")==0){
			whichMode = 2;
		} else if (strcmp(argv[1], "-lru")==0){
			whichMode = 1;
		} else {
			printf(" Didn't recognize your argument please use -lfu or -lru for yor argument\n");
			exit(1);
		}
	}
	//blank globals
	curPages =0;
	numProc = 0;
	numReadFault =0;
	numRWFault = 0;
	lastReplaced = 0;
	numPages = MEMAVAIL/PAGESIZE;
	#if DEBUG
	printf("mask is %x\n",ADDRMASK);
	#endif
	//Create a page table and blank it's values
	pageTable = malloc( sizeof(page) * numPages);
	int i;
	for(i=0; i<numPages; i++){
		pageTable[i].pid = -1;
		pageTable[i].addr = 0;
		pageTable[i].Acc = 0;
		pageTable[i].Dirt = 0;
	}
	#if DEBUG
		printf("%d pages\n", numPages);
	#endif
	//Simulate and report back stats
	Simulate(1000000);
	printf("Number of pure read faults %d, number of Read Write faults %d, and total number of faults %d \n", numReadFault, numRWFault, numReadFault+numRWFault);
	exit(0);

}