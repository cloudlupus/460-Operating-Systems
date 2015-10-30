#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include "Assg2.h"
#include "MemSim.h"

#define MAXPROC 16 // num
#define PAGESIZE 4096 //bytes THIS IS ASSUMING MEBI
#define MEMAVAIL 8388608 //bytes THIS IS ASSUMING MEBI 
#define ADDRSIZE 32 //bits
#define DEBUG 0
#define ADDRMASK ~4096

struct pageData{
	int pid;
	int addr; // first 20 bits are addr, last 12 are offset
	int Acc;
	int Dirt;
};

typedef struct pageData page;

int numReadFault;
int numRWFault;
int numProc;
int curPages;
int numPages;
page* pageTable;

int lastReplaced;

int ContainsPID(int pid){
	int i;
	for(i = 0; i<numPages; i++){
		if(pageTable[i].pid == pid){
			return i;
		}
	}
	return -1;
}

void setEntry(int index, int pid, int address, int write){
	pageTable[index].pid = pid;
	pageTable[index].addr = address;
	pageTable[index].Acc = 1;
	if(write == 1){
		pageTable[index].Dirt = 1;
	}
}

int Find(int pid, int address, int write){
	int startAddr = ContainsPID(pid);
	//If full
	if(numProc == MAXPROC){
		//And we don't already have the PID don't let process start
		if(startAddr == -1){
			printf("~~ can't add another process\n");
			return 0; //Full on proc and can't do this
		}
	}
	if( startAddr == -1){
		numProc++;
	}
	//check for it's currente existance.
	int i;
	int ret = 0;
	for(i = startAddr; i < numPages; i++){
		if(pageTable[i].pid == pid && (pageTable[i].addr & ADDRMASK) == (address & ADDRMASK)){
			pageTable[i].Acc = 1;
			if(write == 1){
				pageTable[i].Dirt = 1;
			}
			printf("~~ Page already exists and we toched it\n");
			ret = 1; // we have the page and touched it.
			break;
		}
	}
	if(ret != 1){
		if(curPages >= numPages){
			//replacement 5 is off the charts, priroity is 1>2>3>4
			int repInd = -1;
			int repType = 5;
			int j;
			for (j = 0; j < numPages; j++){
				int efInd = (j+ lastReplaced) % numPages;
				if( (pageTable[efInd].Acc == 0 && pageTable[efInd].Dirt ==0)
				  && repType > 1 ){
				  	repType = 1;
				  	repInd = efInd;
				  	break;

				} else if( (pageTable[efInd].Acc == 0 && pageTable[efInd].Dirt ==1)
				 && repType > 2){
					repType = 2;
					repInd = efInd;
				} else if ( (pageTable[efInd].Acc == 1 && pageTable[efInd].Dirt ==0)
				 && repType >3){
					repType = 3;
					repInd = efInd;

				} else if ( (pageTable[efInd].Acc == 1 && pageTable[efInd].Dirt ==1)
				 && repType > 4){
				repType = 4;
				repInd = efInd;
				}
			}
			//Replace the table entry;
			if(repInd != -1){
				setEntry(repInd, pid, address, write);
			} else{
				//horrible error
				exit(1);
			}
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
			if(write ==1){
				numRWFault++;
			} else {
				numReadFault++;
			}
			printf("~~ page table full and not already in replaced\n");

		} else {
			//find a space
			curPages++;
			int j;
			for(j = 0; j  < numPages; j++ ){
				int efInd = (j + lastReplaced) % numPages;
				if(pageTable[efInd].pid == -1){
					setEntry(efInd, pid, address, write);
					break;
				}
			}
			printf("~~ Page table had room and we didn't already contain it\n");
		}
	}
	return 1;
}

int Access(int pid, int address, int write){
	printf("Access called with pid %d, addr %d, write %d\n",pid, address, write );
	int cases= Find(pid, address, write);
	printf("~~!returning value %d\n", cases);
	return cases;
}

void Terminate(int givenpid){
	printf("--terminated called with given pid %d\n",givenpid);
	numProc--;
	int i;
	for(i = 0; i< numPages; i++){
		if(pageTable[i].pid == givenpid){
			pageTable[i].pid = -1;
			curPages--;
		}
	}
}

int main(int argc, char* argv[]){
	curPages =0;
	numProc = 0;
	numReadFault =0;
	numRWFault = 0;
	lastReplaced = 0;
	numPages = MEMAVAIL/PAGESIZE;
	pageTable = malloc( sizeof(page) * numPages);
	int i;
	for(i=0; i<numPages; i++){
		pageTable[i].pid = -1;
		pageTable[i].addr = 0;
		pageTable[i].Acc = 0;
		pageTable[i].Dirt = 0;
	}

	#if DEBUG
		for(i = 0; i < numPages; i++){
			printf("for page #%d, the pid is %d, addr is %d, and accdirt is %d\n", i, pageTable[i].pid, pageTable[i].addr, pageTable[i].AccDirt);
		}
	#endif
	printf("%d pages\n", numPages);
	Simulate(100000);
	printf("Number of pure read faults %d, number of Read Write faults %d, and total number of faults %d \n", numReadFault, numRWFault, numReadFault+numRWFault);
	exit(0);

}