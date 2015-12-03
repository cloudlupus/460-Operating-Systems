/*
David Shagam
W01027008
CS 460
Assignment 3 File System.
Assignment interacts with provided driver file to read, write, and delete files as well as format a pseudo File System.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FileSysAPI.h"
#include "Driver.h"

#define DEBUG 1
//BYTES_PER_SECTOR for num bytes in a sector
//SECTORS is num sectors
//Things to call are in drive.h
//things to implement are in FileSysAPI.h

//Data STructure used for the Free list nodes.
typedef struct freeListNode{
	int startInd;
	int size;
	struct freeListNode* next;
} freeNode;

//Data Structure for the Primary INode
typedef struct indexBlock{
	int noInd[13];
	int singleInd;
	int doubleInd;
	int tripleInd;
} indBlock;

//Data STructure used for indirect Nodes same size and interchangeable with indBlock
//However more convenient for the indirect nodes due to them being symmetric 16 operations.
typedef struct arrayBlock{
	int noInd[16];
} arrayBlock;

//Data structure for the file entires in the directory.
typedef struct dirEntry{
	char* name;
	int indBlockLoc;
	struct dirEntry* next;
	struct dirEntry* prev;
} dirEnt;

//Globals
dirEnt *startDir = NULL; // Starting file in my directory
freeNode *freeList = NULL; //starting entry in the linked list.
int numFree = -1; //Used for figuring out if we can fit a file.


//Free list information

//grabs an index from the free list
int freeListRequest(){
	freeNode *cur = freeList;
	//List isn't empty
	if(cur!= NULL){
		//First element in the list has more than 1 value (we can just shrink it)
		if(cur->size > 1){
			cur->size = cur->size -1;
			numFree--;
			return cur->startInd + cur->size - 1;
			//Only one value in hte first thing we have to remove and fix list.
		} else if( cur->size == 1){
			freeNode *temp = cur;
			freeList = cur->next;
			int returnVal = temp->startInd;
			free(temp);
			numFree--;
			return returnVal;
		}
	}
	return -1;
}

//Compacts the list back down.
void coalless(){
	freeNode *cur = freeList;
	while(cur->next != NULL){
		//Check if my index + size = the next thing in the list if so compact and recheck.
		if(cur->next->startInd == (cur->startInd + cur->size)){
			freeNode *temp = cur->next;
			cur->next = temp->next;
			cur->size += temp->size;
			free(temp);
		} else {
			//I'm not able to compact this one so check the next.
			cur = cur->next;
		}
	}

}

//linked list delete helper
//Inserts into the linked list where an index is supposed to go.
void FreeListInsert(int loc){
	int inserted = 0;
	numFree++;
	freeNode *insert = malloc( sizeof(freeNode));
	insert->startInd = loc;
	insert->size = 1;
	insert->next = NULL;
	freeNode *curLoc = freeList;
	//nothing free in the first place.
	//start of list.
	if(curLoc == NULL){
		freeList = insert;
		//start of list.
	} else if(insert->startInd < curLoc->startInd) {
		insert->next = curLoc;
		freeList = insert;
		inserted = 1;
	} else {

		while(curLoc->next != NULL){
		//case middle
			if(insert->startInd < curLoc->next->startInd &&
				insert->startInd > curLoc->startInd ){
				// we live here
				insert->next = curLoc->next;
				curLoc->next = insert;
				inserted = 1;
				break;
			}
			curLoc = curLoc->next;
		}
	}
	if(inserted == 0 && insert->startInd > curLoc->startInd){
		//we live at the end of the list.
		curLoc->next = insert;
		inserted = 1;
	}
	if(inserted == 0){
		printf("We some how didn't find a place to insert this that is a problem\n");
	}

}


//returns -1 for doesn't contain
//else returns the block number of the inode
int dirContains(char *name){
	dirEnt *curLoc = startDir;
	int retVal = -1;
	//checks the entire list until it hits the end.
	while(curLoc!=NULL){
		if(strcmp(curLoc->name, name) == 0){
			retVal = curLoc->indBlockLoc;
			break;
		}
		curLoc = curLoc->next;
	}
	return retVal;
}


//Formats 0 is fail 1 is success
int CSCI460_Format(){
	//Call DevFormat();
	if(DevFormat() == 0){
		return 0;
	} else {
		//create data structures we care about.
		numFree = SECTORS;
		freeList = malloc( sizeof( freeNode ));
		freeList->size = SECTORS;
		freeList->startInd = 0;
		freeList->next = NULL;
	}
	return 1;
}

//Returns -1 on error and returns the index of hte logical block in which the array was stored.
//Takes how many things are left to write, how far into the offset we are, and the Data we need to pull from.
//saves to the file system blocks of data.
int writeSingle(int *numToWrite, int *offset, char* Data ){
	int i;
	int numIts;
	arrayBlock singleInd;
	for(i = 0; i < 16; i++){
		singleInd.noInd[i] = -1;
	}
	int singleIndex = freeListRequest();
	if(singleIndex == -1){
		return -1;
	}
	if(*numToWrite > 16){
		numIts = 16;
	} else {
		numIts = *numToWrite;
	}
	for(i = 0; i < numIts; i++){
		int index = freeListRequest();
		if(index == -1){
			return -1;
		}
		char Buff[BYTES_PER_SECTOR];
		//Filles a write buffer with the data we care about.
		int truncate = snprintf(Buff, BYTES_PER_SECTOR, "%s", Data+ *offset);
		if(truncate > 64){
			*offset+= 64;
		} else {
			*offset+= truncate;
		}
		//sets the single indirection array value to this index.
		singleInd.noInd[i] = index;
		//writes buffer out to file system.
		DevWrite(index, Buff);
		*numToWrite = *numToWrite - 1; 
	}
	//Writes the singleindirect INode out to the file system and returns it's index.
	char arrayWrite[BYTES_PER_SECTOR];
	memcpy(arrayWrite, &singleInd, BYTES_PER_SECTOR);
	DevWrite(singleIndex, arrayWrite);
	return singleIndex;
}

//Writes a Double Indirect to the file system
//Does this by calling writeSingle possibly multiple times.
//returns -1 on error or the index of the DoubleIndirect Inode
//writes data out to file, writes Inodes out to file.
int writeDouble(int *numToWrite, int *offset, char* Data){
	int i;
	int doubleIndex = freeListRequest();
	arrayBlock doubleInd;
	if(doubleIndex == -1){
		return -1;
	}
	for(i = 0; i < 16; i++){
		doubleInd.noInd[i] = -1;
	}
	//populate the first single index in the double index.
	int curDoubIndex = 0;
	while(curDoubIndex < 16 && *numToWrite > 0){
		//THIS CODE IS IDENTICLE TO SINGLE INDIRECT I CAN MAKE HELPER FUNCTIONS
		int index = writeSingle(numToWrite, offset, Data);
		if(index == -1){
			return -1;
		}
		doubleInd.noInd[curDoubIndex] = index;
		curDoubIndex++;

	}
	char arrayWrite[BYTES_PER_SECTOR];
	memcpy(arrayWrite, &doubleInd, BYTES_PER_SECTOR);
	DevWrite(doubleIndex, arrayWrite);
	return doubleIndex;

}

//Write triple same as write double but with one more layer of reptition.
//Same takes and returns, does the same as the other things.
int writeTriple(int *numToWrite, int *offset, char *Data){
	int i;
	int tripleIndex = freeListRequest();
	arrayBlock tripleInd;
	if(tripleIndex == -1){
		return -1;
	}
	for(i = 0; i < 16; i++){
		tripleInd.noInd[i] = -1;
	}
	int curTripIndex = 0;
	while(curTripIndex < 16 && *numToWrite > 0){
		int index = writeDouble(numToWrite, offset, Data);
		if (index == -1){
			return -1;
		}
		tripleInd.noInd[curTripIndex] = index;
		curTripIndex++;
	}
	char arrayWrite[BYTES_PER_SECTOR];
	memcpy(arrayWrite, &tripleInd, BYTES_PER_SECTOR);
	DevWrite(tripleIndex, arrayWrite);
	return tripleIndex;
}

//0 fail 1 success
//Populates an INode and Writes data out to file
//Writes the INodes out to file.
//Adds a new File to the Directory
//Deletes a file with the same name as it if one is found and then writes a new file with the same name.
int CSCI460_Write(char *FileName, int dSize, char *Data){
	//Call DevWrite(//int block#, char* data);
	//Calculate size; 
	int numWhole = (dSize / BYTES_PER_SECTOR);
	int numPartial = dSize % BYTES_PER_SECTOR;
	int numOverhead = 0;
	if(numPartial != 0){
		numWhole++;
	}
	int numToWrite = numWhole;
	//Calculate overhead
	if(numWhole <14){
		//numWhole = numWhole +1;
		numOverhead = 1;

	} else if(numWhole < 30){
		//numWhole = numWhole +2;
		numOverhead = 2;
	} else if(numWhole < 286){
		int whole = (numWhole - 29)/16;
		int rem = (numWhole - 29)%16;
		//numWhole = numWhole + 3 + whole;
		numOverhead = 3 + whole;
		if(rem != 0){
			numOverhead++;
		}
	} else if(numWhole <4382){
		int whole1 = (numWhole - 285)/ 256;
		int rem1 = (numWhole -285)%256;
		int whole2= (numWhole - 285)/256;
		int rem2= (numWhole -285)%256;
		//numWhole = numWhole + 20 + whole1 + whole2;
		numOverhead = 20 + whole1 + whole2;
		if(rem1 != 0){
			numOverhead++;
		}
		if(rem2 != 0){
			numOverhead++;
		}
	}

	//Or if we have a file system.
	if(numFree < (numWhole + numOverhead ) || numFree <= 0){
		return 0;
	}
	//Check for existance
	int cont = dirContains(FileName);
	//If it exists delete it
	if(cont != -1){
		CSCI460_Delete(FileName);
	}
	indBlock INode;
	int i;
	//Init the inode to default values
	for(i =0; i < 13; i++){
		INode.noInd[i] = -1;
	}
	INode.singleInd = -1;
	INode.doubleInd = -1;
	INode.tripleInd = -1;
	//Find where in the free list we can grab the node
	int InodeInd = freeListRequest();
	if(InodeInd == -1){
		return 0;
	}
	//Calculate how many iterations we need.
	int numIts;
	if(numWhole > 13){
		numIts = 13;
	} else {
		numIts = numWhole;
	}
	//keep track of offset into data we have written.
	int offset = 0;
	//no indirection. loops between 1 and 13 times.
	for(i = 0; i < numIts; i++){
		int index = freeListRequest();
		if(index == -1){
			return 0;
		}
		char Buff[BYTES_PER_SECTOR];
		int truncate = snprintf(Buff, BYTES_PER_SECTOR, "%s", Data+offset);
		//NECCESARY BECAUSE SNPRINTF IS SILLY AND RETURNS REMAINDER IF TRUNCATED. Else it's output is how much is written if not truncated.
		if(truncate > 64){
			offset+= 64;
		} else {
			offset+= truncate;
		}
		//write to file.
		INode.noInd[i] = index;
		DevWrite(index, Buff);
		numToWrite--;
	}
	//single indirection.
	if(numWhole > 13){
		int test = writeSingle( &numToWrite, &offset, Data);
		if(test == -1){
			return 0;
		}
		//store the in master INode
		INode.singleInd = test;
	}
	//double indirection
	if(numWhole > 29){
		int test = writeDouble(&numToWrite, &offset, Data);
		if(test == -1){
			return 0;
		}
		INode.doubleInd = test;
	}
	//triple indirection
	if(numWhole > 285){
		int test = writeTriple(&numToWrite, &offset, Data);
		if(test == -1){
			return 0;
		}
		INode.tripleInd = test;
	}
	//Write master Inode out to file.
	char InodeWrite[BYTES_PER_SECTOR];
	memcpy(&InodeWrite, &INode, BYTES_PER_SECTOR);
	DevWrite(InodeInd, InodeWrite);

	//store file descriptor.
	dirEnt *entry = malloc( sizeof(dirEnt));
	entry->name = FileName;
	entry->indBlockLoc = InodeInd;
	entry->prev = NULL;
	if(startDir != NULL){
		entry->next = startDir;
		startDir->prev = entry;
	}
	startDir = entry;

	return 1;
}

//read single indir
//Takes in the Inode location to read, the current index into our buffer, the maximum we are allowed to read, and the buffer to fill
//Filles the buffer with information until we run out or hits max for the single indirect.
int readSingleInd(int inodeLoc, int *curBufind, int maxSize, char *Data){
	char inodeBuff[BYTES_PER_SECTOR];
	int fill = DevRead(inodeLoc, inodeBuff);
	if(fill == 0){
		return 0;
	}
	arrayBlock Inode;
	memcpy(&Inode, inodeBuff, BYTES_PER_SECTOR);

	char Buff[BYTES_PER_SECTOR];
	int i;
	for( i = 0; i < 16; i++){
		//we have filled the entire buffer or end of inode list.
		if(Inode.noInd[i] < 0 || *curBufind >= maxSize){
			break;
		}
		//read info
		int thisPass = DevRead(Inode.noInd[i], Buff);
		//make sure it didn't error.
		if(thisPass == 0){
			return 0;
		}

		//we won't be too large fill all.
		if((thisPass + *curBufind) < maxSize){
			sprintf(Data + *curBufind,"%s", Buff);
			*curBufind += strlen(Buff);
		} else {
			//we are too large fill up until exact fit.
			int j;
			for(j =*curBufind; j< maxSize; j++ ){
				Data[j] = Buff[j-*curBufind];
			}
			//were f ull abort.
			*curBufind = maxSize;
		}
	}
	return *curBufind;
}
//read double which is just a repeated call to read single.
//recursively calls the single Indirect to fill the buffer for double indirect reading
int readDoublInd(int inodeLoc, int *curBufind, int maxSize, char *Data){
	char inodeBuff[BYTES_PER_SECTOR];
	int fill = DevRead(inodeLoc, inodeBuff);
	if(fill == 0){
		return 0;
	}
	arrayBlock Inode;
	memcpy(&Inode, inodeBuff, BYTES_PER_SECTOR);
	int i;
	for(i = 0; i < 16; i++){
		if(Inode.noInd[i] < 0 || *curBufind >= maxSize){
			break;
		}
		int check = readSingleInd(Inode.noInd[i], curBufind, maxSize, Data);
		if (check == 0){
			return 0;
		}
	}

	return *curBufind;
}
//just a repeated call to read double.
//Fills buffer.
int readTripleInd(int inodeLoc, int *curBufind, int maxSize, char *Data){
	char inodeBuff[BYTES_PER_SECTOR];
	int fill = DevRead(inodeLoc, inodeBuff);
	if(fill == 0){
		return 0;
	}
	arrayBlock Inode;
	memcpy(&Inode, inodeBuff, BYTES_PER_SECTOR);
	int i;
	for(i = 0; i < 16; i++){
		if(Inode.noInd[i] < 0 || *curBufind >= maxSize){
			break;
		}
		int check = readDoublInd(Inode.noInd[i], curBufind, maxSize, Data);
		if (check == 0){
			return 0;
		}
	}

	return *curBufind;
}

//0 fail 1 success
//Takes a file name, max size to read, and a buffer to fill.
//Errrors if we can't find hte file, can't read hte file, errors while reading, no file system, etc.
int CSCI460_Read(char *FileName, int maxSize, char *Data){
	//Call DevRead(//int block#, char* buffertofill)
	//no format
	if(numFree == -1){
		return 0;
	}
	//check if it contains
	int startIndex = dirContains(FileName);

	if(startIndex == -1){
		return 0;
	}
	//create the base inode
	char baseInodeBuff[BYTES_PER_SECTOR];
	int fill = DevRead(startIndex, baseInodeBuff);
	indBlock baseInode;
	memcpy(&baseInode, baseInodeBuff, BYTES_PER_SECTOR);
	//no inode or the read errors.
	if(fill == 0){
		return 0;
	}
	int newMax = maxSize -1;
	int curFill = 0;
	char Buff[BYTES_PER_SECTOR];
	//for direct accesses
	int i;
	for(i = 0; i <13; i++){
		//we have filled the entire buffer or end of inode list.
		if(baseInode.noInd[i] < 0 || curFill >= newMax){
			break;
		}
		//read info
		int thisPass = DevRead(baseInode.noInd[i], Buff);
		//make sure it didn't error.
		if(thisPass == 0){
			return 0;
		}
		//we won't be too large fill all.
		if((thisPass + curFill) < newMax){
			sprintf(Data + curFill,"%s", Buff);
			curFill += strlen(Buff);
		} else {
			//we are too large fill up until exact fit.
			int j;
			for(j =curFill; j< newMax; j++ ){
				Data[j] = Buff[j-curFill];
			}
			//were f ull abort.
			curFill = newMax;
		}


	}
	//Call the helper fucntions for reading the Indirects.
	if(curFill < newMax){
		int check;
		if(baseInode.singleInd >= 0){
		  check = readSingleInd(baseInode.singleInd, &curFill, newMax, Data);
		  if(check == 0){
		  	return 0;
		  }
		}
		if(baseInode.doubleInd >= 0){
			check = readDoublInd(baseInode.doubleInd, &curFill, newMax, Data);
			if(check == 0){
				return 0;
			}
		}
		if(baseInode.tripleInd >= 0){
			check = readTripleInd(baseInode.tripleInd, &curFill, newMax, Data);
			if(check == 0){
				return 0;
			}
		}
	}
	//Fill the last location in the string with null character.
	Data[curFill+1]= '\0';
	//Returns how many characters were filled into the buffer not including the null character.
	return curFill;
}

//Deletes single indirect information.
int DeleteSingle(int Index){
	arrayBlock base;
	char Buff[BYTES_PER_SECTOR];
	int fill = DevRead(Index, Buff);
	memcpy(&base, Buff, BYTES_PER_SECTOR);
	if(fill == 0){
		return 0;
	}

	int i;
	for(i =0; i < 16; i++){
		if(base.noInd[i] >= 0){
			FreeListInsert(base.noInd[i]);
		}
	}
	FreeListInsert(Index);
	return 1;
}

//Deletes double indirect by by calling single indirect delete.
int DeleteDouble(int Index){
	arrayBlock base;
	char Buff[BYTES_PER_SECTOR];
	int fill = DevRead(Index, Buff);
	memcpy(&base, Buff, BYTES_PER_SECTOR);
	if(fill == 0){
		return 0;
	}

	int i;
	for(i =0; i < 16; i++){
		if(base.noInd[i] >= 0){
			DeleteSingle(base.noInd[i]);
		}
	}
	FreeListInsert(Index);
	return 1;
}

//deletes triple indirect by calling double indirect delete
int DeleteTriple(int Index){
	arrayBlock base;
	char Buff[BYTES_PER_SECTOR];
	int fill = DevRead(Index, Buff);
	memcpy(&base, Buff, BYTES_PER_SECTOR);
	if(fill == 0){
		return 0;
	}

	int i;
	for(i =0; i < 16; i++){
		if(base.noInd[i] >= 0){
			DeleteDouble(base.noInd[i]);
		}
	}
	FreeListInsert(Index);
	return 1;
}

//0 fail 1 success
//looks for a filename, if it contains it it deletes it
//errors if file doesn't exist, if it fails to delete hte file, or no file system.
//edits freespace and the directory information.
int CSCI460_Delete( char* FileName){
	if(numFree == -1){
		return 0;
	}

	//check if it contains
	int startIndex = -1;
	dirEnt *cur = startDir;
	while(cur != NULL){
		if(strcmp(FileName, cur->name) == 0){
			startIndex = cur->indBlockLoc;
			//start of list
			if(cur->prev == NULL){
				startDir = cur->next;
			} else {
				cur->prev->next = cur->next;
			}
			free(cur);
			break;
		}
		cur = cur->next;
	}

	if(startIndex == -1){
		return 0;
	}
	//Read the INode so we can traverse it.
	indBlock base;
	char Buff[BYTES_PER_SECTOR];
	int fill = DevRead(startIndex, Buff);
	memcpy(&base, Buff, BYTES_PER_SECTOR);
	if(fill == 0){
		return 0;
	}
	//Delete no indirections.
	int i;
	for(i =0; i < 13; i++){
		if(base.noInd[i] >= 0){
			FreeListInsert(base.noInd[i]);
		}
	}
	//delete single indirections
	if(base.singleInd >= 0){
		DeleteSingle(base.singleInd);
	}
	//deletes double indirections
	if( base.doubleInd >= 0){
		DeleteDouble(base.doubleInd);
	}
	//deletes triple indirections.
	if(base.tripleInd >= 0){
		DeleteTriple(base.tripleInd);
	}

	//deletes the INode
	FreeListInsert(startIndex);
	//compacts the free list.
	coalless();

	return 1;

}

