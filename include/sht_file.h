#ifndef SHT_FILE_H
#define SHT_FILE_H

#include "hash_file.h"

typedef struct SHT
{
	int depth;
	int buckets[MAX_BUCKETS];
	int nextHT;				  // pointer to the next secondary hash table
	char attrName[20];		  // which field
	int attrlen;			  // sizeof(attr)
	char primary_HT[50];
} SHT;

typedef struct SecondaryRecord
{
	char index_key[20];
	int tupleId;
} SecondaryRecord;

typedef struct SecondaryBucket // bucket for secondary hash table
{
	int recordCount;
	int localDepth;
	SecondaryRecord records[21]; // 21 is the maximum that BF block can fit
} SecondaryBucket;

HT_ErrorCode SHT_Init();

HT_ErrorCode SHT_CreateSecondaryIndex(
	const char *sfileName, /* file name */
	char *attrName,		   /* attribute name */
	int attrLength,		   /* attribute length */
	int depth,			   /* global depth */
	char *fileName 		   /* primary hashtable name */);

HT_ErrorCode SHT_OpenSecondaryIndex(
	const char *sfileName, 
	int *indexDesc );

HT_ErrorCode SHT_CloseSecondaryIndex(int indexDesc);

HT_ErrorCode SHT_SecondaryInsertEntry(
	int indexDesc,
	SecondaryRecord record );

HT_ErrorCode SHT_SecondaryUpdateEntry(
	int indexDesc,
	UpdateRecordArray *updateArray);

HT_ErrorCode SHT_PrintAllEntries(
	int sindexDesc, 
	char *index_key);

HT_ErrorCode SHT_HashStatistics(char *filename );

HT_ErrorCode SHT_InnerJoin(
	int sindexDesc1, 
	int sindexDesc2, 
	char *index_key);

#endif // HASH_FILE_H