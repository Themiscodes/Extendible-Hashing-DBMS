#ifndef HASH_FILE_H
#define HASH_FILE_H

#include <stdbool.h>

typedef enum HT_ErrorCode {
  HT_OK,
  HT_ERROR
} HT_ErrorCode;

#define MAX_OPEN_FILES 20
#define MAX_RECORDS 8 // meaning BF_BLOCK_SIZE / sizeof(Record)
#define MAX_BUCKETS 64

typedef struct Record {
	int id;
	char name[15];
	char surname[20];
	char city[20];
} Record;

typedef struct Index{ // file information
	int fileCount;
	int fileDesc[MAX_OPEN_FILES];
} Index;

typedef struct Bucket{
  int recordCount;
  int localDepth;
  Record records[MAX_RECORDS]; 
} Bucket;

typedef struct HashTable{
  int depth; 
  int buckets[MAX_BUCKETS];
  int nextHT; // pointer to the next hashtable
} HashTable;

typedef struct UpdatedRecord{  
	char surname[20];
	char city[20];
	int oldTupleId;
	int newTupleId;
} UpdatedRecord;

// keeping the changes in the array
typedef struct UpdateRecordArray{  
	int howManyUpdates; 
	UpdatedRecord updatedRecord[9]; // since 9 are the maximum changes cause by a split
} UpdateRecordArray;

int hashFunction(int id, int depth); 


// a Linked List to be used for short-term purposes
typedef struct LL {
    int HTblock;
    struct LL* next;
} LL;

void insertLL(int hblock, LL** head);

bool inLL(LL* head, int blocknum);

void freeLL(LL* head);


/* 
 * HT_Init is used to initialise the data structures. 
 * Returns HT_OK on success
*/
HT_ErrorCode HT_Init();

/*
  * The HT_CreateIndex function is used to create and appropriately initialize an empty hash file named fileName.
  * If the file already exists, then an error code is returned.
  * If executed successfully, HT_OK is returned, otherwise an error code is returned.
 */
HT_ErrorCode HT_CreateIndex(
	const char *fileName,
	int depth
);

/*
* This routine opens the file named fileName.
  * If the file is opened normally, the routine returns HT_OK, otherwise an error code.
 */
HT_ErrorCode HT_OpenIndex(
	const char *fileName, 	
  	int *indexDesc        	/* position in open files array returned */
);

/*
  * This routine closes the file whose information is at position indexDesc of the open file table.
  * Also deletes the entry corresponding to this file in the table of open files.
  * The function returns HT_OK if the file is closed successfully, otherwise an error code.
 */
HT_ErrorCode HT_CloseFile(
	int indexDesc 			/* position in open files array returned */
	);

/*
  * The HT_InsertEntry function is used to insert an entry into the hash file.
  * Information about the file is in the open files table, while the record to import is specified by the record structure.
  * If executed successfully, HT_OK is returned, otherwise some error code.
 */
HT_ErrorCode HT_InsertEntry(
	int indexDesc,		/* position in open files array returned */
	Record record,		/* record structure */
	int *tupleID,
	UpdateRecordArray *updateArray
	);

/*
  * The HT_PrintAllEntries function is used to print all records for which record.id has an id value.
  * If id is NULL then it will print all records of the hash file.
  * If executed successfully, HP_OK is returned, otherwise some error code.
 */
HT_ErrorCode HT_PrintAllEntries(
	int indexDesc,			/* position in open files array returned */
	int *id 				/* id that is searched */
	);

HT_ErrorCode HashStatistics(char* fileName);

#endif // HASH_FILE_H
