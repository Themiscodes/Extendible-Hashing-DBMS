#include "hash_file.h"
#include "bf.h"
#include <math.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALL_BF(call)             \
    {                             \
        BF_ErrorCode code = call; \
        if (code != BF_OK) {      \
            BF_PrintError(code);  \
            return HT_ERROR;      \
        }                         \
    }

static Index indexTable; // index table for the open files


// the Linked List Functions
void insertLL(int hblock, LL** head)
{
    while (*head) {
        head = &(*head)->next;
    }

    (*head) = malloc(sizeof(LL));
    (*head)->HTblock = hblock;
    (*head)->next = NULL;
}

bool inLL(LL* head, int blocknum)
{
    while (head) {
        if (head->HTblock == blocknum) {
            return true;
        }
        head = head->next;
    }
    return false;
}

void freeLL(LL* head)
{
    while (head) {
        LL* u = head;
        head = head->next;
        free(u);
    }
}

// the hash function to accomodate the buddy system
int hashFunction(int id, int depth){
  int index =id;
  int reversi =0;
  int allbits=32;
  while (allbits--){        
    
    // reverse the bits
    reversi = (reversi<<1) | (index & 1);
    index>>=1;
  
  }

  // get the last depth bits
  reversi = reversi >>(32-depth); 
  int reversi2 =0;
  while (depth--){ 
    
    // to be read from the end for the buddy system to work
    reversi2 = (reversi2<<1) | (reversi & 1);
    reversi>>=1;
  }
  return reversi2;
}

HT_ErrorCode HT_Init()
{
    CALL_BF(BF_Init(LRU));
    indexTable.fileCount = 0;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        indexTable.fileDesc[i] = -1;
    }
    return HT_OK;
}

HT_ErrorCode HT_CreateIndex(const char* filename, int depth)
{

    if (indexTable.fileCount == MAX_OPEN_FILES)
        return HT_ERROR; // if the open files haven't reached the maximum allowed

    int fd1;
    BF_Block* hashBlock;
    BF_Block_Init(&hashBlock);
    CALL_BF(BF_CreateFile(filename));
    CALL_BF(BF_OpenFile(filename, &fd1));

    // first block hashtable information, the rest just buckets
    char* data;
    CALL_BF(BF_AllocateBlock(fd1, hashBlock));
    data = BF_Block_GetData(hashBlock);
    HashTable hashuTaburu;
    hashuTaburu.depth = depth;
    hashuTaburu.nextHT = -1; // to know this is the end
    for (int i = 0; i < 64; i++) {
        hashuTaburu.buckets[i] = -1; // to know this is empty
    }
    memcpy(data, &hashuTaburu, sizeof(HashTable));

    BF_Block_SetDirty(hashBlock);
    CALL_BF(BF_UnpinBlock(hashBlock));
    CALL_BF(BF_CloseFile(fd1));

    return HT_OK;
}

HT_ErrorCode HT_OpenIndex(const char* fileName, int* indexDesc)
{
    if (indexTable.fileCount == MAX_OPEN_FILES)
        return HT_ERROR;
    int fd;
    CALL_BF(BF_OpenFile(fileName, &fd));
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        // adding the information in the indexTable
        if (indexTable.fileDesc[i] == -1) {
            indexTable.fileDesc[i] = fd;
            indexTable.fileCount += 1; // added a file
            *indexDesc = i;
            return HT_OK;
        }
    }
    return HT_ERROR;
}

HT_ErrorCode HT_CloseFile(int indexDesc){

    if ((indexDesc < MAX_OPEN_FILES) && (indexDesc > -1) && (indexTable.fileDesc[indexDesc] != -1)) {
        CALL_BF(BF_CloseFile(indexTable.fileDesc[indexDesc])); // close the file
        indexTable.fileDesc[indexDesc] = -1; 
        indexTable.fileCount -= 1;
        return HT_OK;
    }
    return HT_ERROR;
}

HT_ErrorCode HT_InsertEntry(int indexDesc, Record record, int *tupleID, UpdateRecordArray *updateArray) {
  int fileDesc;
  int holdthis = indexDesc;
  
  if ((indexDesc < MAX_OPEN_FILES) && (indexDesc > -1) && (indexTable.fileDesc[indexDesc] != -1))
  {
    fileDesc = indexTable.fileDesc[indexDesc];
  }
  else
  {
    return HT_ERROR;
  }
  
  char *hashuTaburu;
  BF_Block *hashBlock;
  BF_Block_Init(&hashBlock);
  CALL_BF(BF_GetBlock(fileDesc, 0, hashBlock)); // first block is the hashtable
  hashuTaburu = BF_Block_GetData(hashBlock);

  // hash to find the position
  int whereIsMyPlace = hashFunction(record.id, ((HashTable *)hashuTaburu)->depth);
  
  int whichHashTableDoIBelongTo = whereIsMyPlace / 64;
  for (int i = 0; i < whichHashTableDoIBelongTo; i++)
  {
    int nextpos = ((HashTable *)hashuTaburu)->nextHT;
    CALL_BF(BF_UnpinBlock(hashBlock));
    CALL_BF(BF_GetBlock(fileDesc, nextpos, hashBlock));
    hashuTaburu = BF_Block_GetData(hashBlock);
  }
  int bucketDesc = ((HashTable *)hashuTaburu)->buckets[whereIsMyPlace % 64];

  if (bucketDesc == -1){ //case where a new bucket is needed
    BF_Block *litoBucket;
    BF_Block_Init(&litoBucket);
    char *data;
    CALL_BF(BF_AllocateBlock(fileDesc, litoBucket));
    data = BF_Block_GetData(litoBucket);
    Bucket bucketino;
    bucketino.records[0] = record;
    bucketino.recordCount = 1;
    bucketino.localDepth = ((HashTable *)hashuTaburu)->depth; // since one block for now will point to this bucket
    memcpy(data, &bucketino, sizeof(Bucket));

    BF_Block_SetDirty(litoBucket);
    CALL_BF(BF_UnpinBlock(litoBucket));

    int newBlockCounter;
    CALL_BF(BF_GetBlockCounter(fileDesc, &newBlockCounter));
    ((HashTable *)hashuTaburu)->buckets[whereIsMyPlace % 64] = newBlockCounter - 1;

    // update records for Secondary Hashtable
    UpdatedRecord updatedRecord;
    strcpy(updatedRecord.city, record.city);
    strcpy(updatedRecord.surname, record.surname);
    updatedRecord.oldTupleId =newBlockCounter*MAX_RECORDS;
    updatedRecord.newTupleId = newBlockCounter*MAX_RECORDS; // first element so index is 0
    updateArray->howManyUpdates = 1;
    updateArray->updatedRecord[0] = updatedRecord;
    *tupleID = updatedRecord.newTupleId;
    
    BF_Block_SetDirty(hashBlock);      // these have been changed
    CALL_BF(BF_UnpinBlock(hashBlock)); // unpin block where hashtable resides

    return HT_OK;
  }
  else
  { // if the bucket already exists
    
    char *bucketData;
    BF_Block *bucketBlock;
    BF_Block_Init(&bucketBlock);
    CALL_BF(BF_GetBlock(fileDesc, bucketDesc, bucketBlock));
    bucketData = BF_Block_GetData(bucketBlock);
    // if it's full
    if (((Bucket *)bucketData)->recordCount == 8){
      if (((HashTable *)hashuTaburu)->depth > ((Bucket *)bucketData)->localDepth)
      { // Bucket splitting
        
        // check if they don't hash in the same
        int count =0;
        for (int i = 0; i < ((Bucket *)bucketData)->recordCount; i++){
          Record r = ((Bucket *)bucketData)->records[i];
          int whereNew = hashFunction(r.id, ((HashTable *)hashuTaburu)->depth);
          if (whereNew == whereIsMyPlace) count++;
        }
        if (count==8){
          
          // update the local depth
          ((Bucket *)bucketData)->localDepth = ((HashTable *)hashuTaburu)->depth;
          
          // update the hash table pointers to the bucket
          int parseThrough = (int)pow(2.0, (double)(((HashTable *)hashuTaburu)->depth));
          if (parseThrough > 64)
          {
            parseThrough = 64;
          }
          char* hashu;
          BF_Block *helpingBlock;
          BF_Block_Init(&helpingBlock);
          int HTindex=0;
          do{
            CALL_BF(BF_GetBlock(fileDesc, HTindex, helpingBlock));
            hashu = BF_Block_GetData(helpingBlock);
            for (int jj=0;jj<parseThrough;jj++){
              if (((HashTable *)hashu)->buckets[jj]== bucketDesc&& jj!=whereIsMyPlace){
                // remove the pointer to the bucket
                ((HashTable *)hashu)->buckets[jj]=-1;
              }
            }
            HTindex = ((HashTable *)hashu)->nextHT;
            BF_Block_SetDirty(helpingBlock);
            CALL_BF(BF_UnpinBlock(helpingBlock));
            if (HTindex!=-1) BF_UnpinBlock(helpingBlock);
          }while(HTindex!=-1);
          BF_Block_SetDirty(bucketBlock);
          BF_UnpinBlock(bucketBlock);
          BF_Block_SetDirty(hashBlock);
          BF_UnpinBlock(hashBlock);

          // recursively call insert to cause a doubling of size
          return HT_InsertEntry(holdthis, record, tupleID, updateArray);
        }

        BF_Block *litoBucket;
        BF_Block_Init(&litoBucket);
        char *data;
        CALL_BF(BF_AllocateBlock(fileDesc, litoBucket));
        data = BF_Block_GetData(litoBucket);
        Bucket bucketino;
        Bucket tempBucket;
        // initialise the new buckets
        int bucketino_records = 0;
        int bucketdata_records = 0;
        int oldBucketPosition = ((HashTable *)hashuTaburu)->buckets[whereIsMyPlace % 64];
        int originalPositions[8];
        for (int i = 0; i < ((Bucket *)bucketData)->recordCount; i++)
        {
          Record r = ((Bucket *)bucketData)->records[i];
          int whereNew = hashFunction(r.id, ((HashTable *)hashuTaburu)->depth);
          originalPositions[i]=r.id; // keep this for update
          if (whereNew == whereIsMyPlace)
          { // old block
            tempBucket.records[bucketdata_records] = r;
            tempBucket.recordCount = ++bucketdata_records;
          }
          else
          { //new block
            bucketino.records[bucketino_records] = r;
            bucketino.recordCount = ++bucketino_records;
          }
        }

        // insert the new record
        int whereNew = hashFunction(record.id, ((HashTable *)hashuTaburu)->depth);
        int indexForSecondaryUpdate = 0;
        int newrecordgoestoold =1;
        if (whereNew == whereIsMyPlace)
        { // old block
          tempBucket.records[bucketdata_records] = record;
          indexForSecondaryUpdate=bucketdata_records;
          tempBucket.recordCount = ++bucketdata_records;
        }
        else
        { //new block
          newrecordgoestoold = 0;
          bucketino.records[bucketino_records] = record;
          indexForSecondaryUpdate=bucketino_records;
          bucketino.recordCount = ++bucketino_records;
        }

        tempBucket.localDepth =  0; // these are adjusted further down
        bucketino.localDepth = 0;
        memcpy(bucketData, &tempBucket, sizeof(Bucket));
        memcpy(data, &bucketino, sizeof(Bucket));

        int newBlockCounter;
        CALL_BF(BF_GetBlockCounter(fileDesc, &newBlockCounter));
        int newBucketPosition = newBlockCounter - 1;

        // update records of secondary hashtable
        UpdatedRecord updatedRecord1;
        strcpy(updatedRecord1.city, record.city);
        strcpy(updatedRecord1.surname, record.surname);
        if (newrecordgoestoold){
          updatedRecord1.newTupleId = (oldBucketPosition+1)*MAX_RECORDS+indexForSecondaryUpdate; 
          updatedRecord1.oldTupleId =updatedRecord1.newTupleId;
          *tupleID = updatedRecord1.newTupleId; // record tuple ID
        }
        else
        {
          updatedRecord1.newTupleId = (newBucketPosition+1)*MAX_RECORDS+indexForSecondaryUpdate; 
          *tupleID = (newBucketPosition+1)*MAX_RECORDS+indexForSecondaryUpdate;  // record tuple id
        }
        
        updateArray->howManyUpdates = 1;
        updateArray->updatedRecord[0] = updatedRecord1;

        // update pointers
        int K = 0; // counter to how many point to the bucket now
        int arrows[8];
        for (int j = 0; j < 8; j++){
          arrows[j] = -1;
        }
        for (int j = 0; j < ((Bucket *)bucketData)->recordCount; j++){
          char *whichHashTablePointsToMe;
          BF_Block *hashBlock3;
          BF_Block_Init(&hashBlock3);
          int whoPointsToMe = hashFunction(((Bucket *)bucketData)->records[j].id, ((HashTable *)hashuTaburu)->depth);
          int unique = 1;
          for (int i = 0; i < 8; i++)
          {
            if (arrows[i] == whoPointsToMe)
              unique = 0;
          }
          if (unique == 1)
          {
            arrows[j] = whoPointsToMe;
            K++;
          }
          CALL_BF(BF_GetBlock(fileDesc, 0, hashBlock3)); // first block hashtable
          whichHashTablePointsToMe = BF_Block_GetData(hashBlock3);
          for (int i = 0; i < whoPointsToMe / 64; i++)
          {
            int nextpos = ((HashTable *)whichHashTablePointsToMe)->nextHT;
            CALL_BF(BF_UnpinBlock(hashBlock));
            CALL_BF(BF_GetBlock(fileDesc, nextpos, hashBlock3));
            whichHashTablePointsToMe = BF_Block_GetData(hashBlock3);
          }
          ((HashTable *)whichHashTablePointsToMe)->buckets[whoPointsToMe % 64] = oldBucketPosition;
          
          // update record array for secondary hash table
          if (((Bucket *)bucketData)->records[j].id!=record.id){
            UpdatedRecord updatedRecord;
            strcpy(updatedRecord.city, ((Bucket *)bucketData)->records[j].city);
            strcpy(updatedRecord.surname, ((Bucket *)bucketData)->records[j].surname);
            for (int li = 0 ;li<8;li++){
              if (originalPositions[li]==((Bucket *)bucketData)->records[j].id){
                updatedRecord.oldTupleId = (oldBucketPosition+1)*MAX_RECORDS+li;
              }
            }
            updatedRecord.newTupleId = (oldBucketPosition+1)*MAX_RECORDS+j; 
            updateArray->updatedRecord[updateArray->howManyUpdates] = updatedRecord;
            updateArray->howManyUpdates += 1;
            BF_Block_SetDirty(hashBlock3);
            CALL_BF(BF_UnpinBlock(hashBlock3));
          }
        }
        ((Bucket *)bucketData)->localDepth = ((HashTable *)hashuTaburu)->depth - (int)log2(K);

        // new bucket
        K = 0;
        for (int j = 0; j < 8; j++)
        {
          arrows[j] = -1;
        }
        for (int j = 0; j < ((Bucket *)data)->recordCount; j++)
        {
          char *whichHashTablePointsToMe;
          BF_Block *hashBlock3;
          BF_Block_Init(&hashBlock3);
          int whoPointsToMe = hashFunction(((Bucket *)data)->records[j].id, ((HashTable *)hashuTaburu)->depth);
          int unique = 1;
          for (int i = 0; i < 8; i++)
          {
            if (arrows[i] == whoPointsToMe)
              unique = 0;
          }
          if (unique == 1)
          {
            arrows[j] = whoPointsToMe;
            K++;
          }
          CALL_BF(BF_GetBlock(fileDesc, 0, hashBlock3)); // first block that is the hashtable
          whichHashTablePointsToMe = BF_Block_GetData(hashBlock3);
          for (int i = 0; i < whoPointsToMe / 64; i++)
          {
            int nextpos = ((HashTable *)whichHashTablePointsToMe)->nextHT;
            CALL_BF(BF_UnpinBlock(hashBlock));
            CALL_BF(BF_GetBlock(fileDesc, nextpos, hashBlock3)); // first block that is the hashtable
            whichHashTablePointsToMe = BF_Block_GetData(hashBlock3);
          }
          ((HashTable *)whichHashTablePointsToMe)->buckets[whoPointsToMe % 64] = newBucketPosition;
          
          // update records
          if (record.id!=((Bucket *)data)->records[j].id){
            UpdatedRecord updatedRecord;
            strcpy(updatedRecord.city, ((Bucket *)data)->records[j].city);
            strcpy(updatedRecord.surname, ((Bucket *)data)->records[j].surname);
            for (int li = 0 ;li<8;li++){
              if (originalPositions[li]==((Bucket *)data)->records[j].id){
                updatedRecord.oldTupleId = (oldBucketPosition+1)*MAX_RECORDS+li;
              }
            }
            updatedRecord.newTupleId = (newBucketPosition+1)*MAX_RECORDS+j; 
            updateArray->updatedRecord[updateArray->howManyUpdates] = updatedRecord;
            updateArray->howManyUpdates += 1;
          }
          
          BF_Block_SetDirty(hashBlock3);
          CALL_BF(BF_UnpinBlock(hashBlock3));
        }                                    
        ((Bucket *)data)->localDepth = ((HashTable *)hashuTaburu)->depth - (int)log2(K);
        BF_Block_SetDirty(litoBucket);
        CALL_BF(BF_UnpinBlock(litoBucket));
        BF_Block_SetDirty(bucketBlock);
        CALL_BF(BF_UnpinBlock(bucketBlock));
        BF_Block_SetDirty(hashBlock);
        CALL_BF(BF_UnpinBlock(hashBlock));
        return HT_OK;
      }
      else if (((HashTable *)hashuTaburu)->depth == ((Bucket *)bucketData)->localDepth)
      { // double the hash table size
        int new_depth = ((HashTable *)hashuTaburu)->depth + 1;
        int newBlockCounter;
        if (new_depth > 6) { // 6 because 2^6=64 size of one structure in this model
          int howManyHTsToMake = (int)pow(2.0, (double)(new_depth - 7));
          char *hashuToLink;
          BF_Block *helpingBlock;
          BF_Block_Init(&helpingBlock);
          int HTindex=0;
          do{
            CALL_BF(BF_GetBlock(fileDesc, HTindex, helpingBlock));
            hashuToLink = BF_Block_GetData(helpingBlock);
            HTindex = ((HashTable *)hashuToLink)->nextHT;
            if (HTindex!=-1) BF_UnpinBlock(helpingBlock);
          }while(HTindex!=-1);
          for (int i = 0; i < howManyHTsToMake; i++){ // create new HTs if needed
            BF_Block *newHashBlock;
            BF_Block_Init(&newHashBlock);
            char *freshData;
            CALL_BF(BF_AllocateBlock(fileDesc, newHashBlock));
            freshData = BF_Block_GetData(newHashBlock);
            HashTable newHashuTaburu;
            newHashuTaburu.depth = ((HashTable *)hashuToLink)->depth; // keep the old depth
            newHashuTaburu.nextHT = -1;                               // to highlight the end
            for (int i = 0; i < 64; i++)
            {
              newHashuTaburu.buckets[i] = -1; // that it's empty
            }
            memcpy(freshData, &newHashuTaburu, sizeof(HashTable));
            BF_Block_SetDirty(newHashBlock);
            CALL_BF(BF_UnpinBlock(newHashBlock));
            CALL_BF(BF_GetBlockCounter(fileDesc, &newBlockCounter));
            ((HashTable *)hashuToLink)->nextHT = newBlockCounter - 1;
            BF_Block_SetDirty(helpingBlock);
            CALL_BF(BF_UnpinBlock(helpingBlock));
            HTindex = newBlockCounter - 1;
            CALL_BF(BF_GetBlock(fileDesc, HTindex, helpingBlock));
            hashuToLink = BF_Block_GetData(helpingBlock);
          }
          BF_Block_SetDirty(helpingBlock);
          CALL_BF(BF_UnpinBlock(helpingBlock));
        }

        // use the buddy system to make the pointers
        char *hashuTaburuChanges;
        char *temporaryHT;
        int newSize = (int)pow(2.0, (double)new_depth);
        int HTindex = 0;
        int hashtableIndicator = 0;
        BF_Block *hashBlock2;
        BF_Block_Init(&hashBlock2);
        BF_Block *helpingHashBlock;
        BF_Block_Init(&helpingHashBlock);

        do
        {
          CALL_BF(BF_GetBlock(fileDesc, HTindex, hashBlock2)); // first block
          hashuTaburuChanges = BF_Block_GetData(hashBlock2);
          ((HashTable *)hashuTaburuChanges)->depth = new_depth;
          int parseThrough = newSize > 64 ? 64 : newSize;
        
          for (int index = 0; index < parseThrough; index++){
            // old HT pointers to the new using the buddies
            int buddyIndex = index + newSize;
            int whichHTbuddyBelongsTo = buddyIndex/64;
            
            int HTindexHelping=0;
            CALL_BF(BF_GetBlock(fileDesc, HTindexHelping, helpingHashBlock));
            temporaryHT =  BF_Block_GetData(helpingHashBlock);
            HTindexHelping = ((HashTable *)temporaryHT)->nextHT;
            
            for (int g =0; g<whichHTbuddyBelongsTo-1; g++){
              CALL_BF(BF_UnpinBlock(helpingHashBlock));
              CALL_BF(BF_GetBlock(fileDesc, HTindexHelping, helpingHashBlock));
              temporaryHT =  BF_Block_GetData(helpingHashBlock);
              HTindexHelping = ((HashTable *)temporaryHT)->nextHT;
            }
            // buddy of power two points to the same bucket
            ((HashTable *)temporaryHT)->buckets[buddyIndex%64] = ((HashTable *)hashuTaburuChanges)->buckets[index];
            BF_Block_SetDirty(helpingHashBlock);
            CALL_BF(BF_UnpinBlock(helpingHashBlock));

          }
          HTindex = ((HashTable *)hashuTaburuChanges)->nextHT;
          hashtableIndicator+=1;
          BF_Block_SetDirty(hashBlock2);
          CALL_BF(BF_UnpinBlock(hashBlock2));

        } while (HTindex != -1);

        BF_Block_SetDirty(hashBlock2);
        BF_UnpinBlock(hashBlock2);
        BF_Block_SetDirty(helpingHashBlock);
        BF_UnpinBlock(helpingHashBlock);
        BF_Block_SetDirty(bucketBlock);
        BF_UnpinBlock(bucketBlock);
        BF_Block_SetDirty(hashBlock);
        BF_UnpinBlock(hashBlock);
        return HT_InsertEntry(holdthis, record, tupleID, updateArray); 
      }
      else
      {
        return HT_ERROR; // if there was an error with the depths
      }
    }
    else
    { // if the bucket had space just place it inside
      int newIndexofRec = ((Bucket *)bucketData)->recordCount;
      ((Bucket *)bucketData)->records[newIndexofRec] = record;
      ((Bucket *)bucketData)->recordCount += 1;

      // update records for secondary
      UpdatedRecord updatedRecord;
      strcpy(updatedRecord.city, record.city);
      strcpy(updatedRecord.surname, record.surname);
      
      //bucketDesc is blockid
      updatedRecord.newTupleId = (bucketDesc+1)*MAX_RECORDS+newIndexofRec;
      updatedRecord.oldTupleId = (bucketDesc+1)*MAX_RECORDS+newIndexofRec;
      updateArray->howManyUpdates = 1;
      updateArray->updatedRecord[0] = updatedRecord;
      *tupleID = updatedRecord.newTupleId;

      BF_Block_SetDirty(bucketBlock);
      CALL_BF(BF_UnpinBlock(bucketBlock));
      BF_Block_SetDirty(hashBlock);
      CALL_BF(BF_UnpinBlock(hashBlock));
      return HT_OK;
    }
  }
  return HT_ERROR;
}

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int* id) 
{
 
    int fileDesc;
    if ((indexDesc < MAX_OPEN_FILES) && (indexDesc > -1) && (indexTable.fileDesc[indexDesc] != -1)) {
        fileDesc = indexTable.fileDesc[indexDesc];
    } else
        return HT_ERROR;

    int num_of_blocks;
    CALL_BF(BF_GetBlockCounter(fileDesc, &num_of_blocks));
    if(num_of_blocks == 1){
        return HT_ERROR;
    }

    BF_Block* hashBlock;
    BF_Block_Init(&hashBlock);
    CALL_BF(BF_GetBlock(fileDesc, 0, hashBlock));
    char* hashuTaburu = BF_Block_GetData(hashBlock);

    if (id != NULL) {
        int whereIsMyPlace = hashFunction(*id, ((HashTable*)hashuTaburu)->depth);
        BF_Block* bucket;
        BF_Block_Init(&bucket);
        int whichHashTableDoIBelongTo = whereIsMyPlace / 64;
        for (int i = 0; i < whichHashTableDoIBelongTo; i++) {
            CALL_BF(
                BF_GetBlock(fileDesc, ((HashTable*)hashuTaburu)->nextHT, hashBlock));
            hashuTaburu = BF_Block_GetData(hashBlock);
        }
        int whichHashTableSlot = whereIsMyPlace % 64;
        int whichfblock = ((HashTable*)hashuTaburu)->buckets[whichHashTableSlot];
        if (whichfblock == -1) {
            printf("ID doesn't exist\n");
            return HT_OK;
        }
        CALL_BF(BF_GetBlock(fileDesc, whichfblock, bucket));
        char* data = BF_Block_GetData(bucket);
        for (int i = 0; i < ((Bucket*)data)->recordCount; i++) {
            Record r = ((Bucket*)data)->records[i];
            if (r.id == *id) {
                printf("ID: %d, name: %s, surname: %s, city: %s\n", r.id, r.name,
                    r.surname, r.city);
            }
        }
        BF_UnpinBlock(bucket);
    } else {
        LL* explorer = NULL; //traverse blocks
        int HT_block = 0;
        do {
            insertLL(HT_block, &explorer); //get which blocks are hashblocks
            CALL_BF(BF_GetBlock(fileDesc, HT_block, hashBlock));
            char* hashuTaburu = BF_Block_GetData(hashBlock);
            HT_block = ((HashTable*)hashuTaburu)->nextHT;
            BF_UnpinBlock(hashBlock);
        } while (HT_block != -1); // no more hash table puzzle pieces
        BF_Block* bucketBlock;
        int howManyBlocks;
        BF_GetBlockCounter(fileDesc, &howManyBlocks);
        BF_Block_Init(&bucketBlock);
        for (int i = 0; i < howManyBlocks; i++) {
            if (!inLL(explorer, i)) { //if not hash block
                CALL_BF(BF_GetBlock(fileDesc, i, bucketBlock));
                char* bucket = BF_Block_GetData(bucketBlock);
                for (int j = 0; j < ((Bucket*)bucket)->recordCount; j++) {
                    Record r = ((Bucket*)bucket)->records[j];
                    printf("ID: %d, name: %s, surname: %s, city: %s\n", r.id, r.name,
                        r.surname, r.city);
                }
                BF_UnpinBlock(bucketBlock);
            }
        }

        freeLL(explorer);
    }
    return HT_OK;   
}

HT_ErrorCode HashStatistics(char* fileName)
{
    int fileDesc, indexDesc;
    HT_OpenIndex(fileName, &indexDesc);
    if ((indexDesc < MAX_OPEN_FILES) && (indexDesc > -1) && (indexTable.fileDesc[indexDesc] != -1)) {
        fileDesc = indexTable.fileDesc[indexDesc];
    } else
        return HT_ERROR;

    // compute the number of blocks in the file
    int num_of_blocks;
    CALL_BF(BF_GetBlockCounter(fileDesc, &num_of_blocks));
    printf("File '%s' has %d Blocks\n", fileName, num_of_blocks);
    if(num_of_blocks == 1){
        printf("No data yet in the file!\n");
        return HT_OK;
    }

    int total_buckets = 0; // counter for buckets
    int total_records = 0; // counter for records
    int min_records = BF_BLOCK_SIZE / sizeof(Record); // max records + 1
    int max_records = 0; // min records per bucket - 1

    char* hashuTaburu;
    BF_Block* hashBlock;
    BF_Block_Init(&hashBlock);

    int HTindex = 0; // first block
    LL* explorer = NULL; // traverse blocks

    do {
        insertLL(HTindex, &explorer); //get which blocks are hashblocks
        CALL_BF(BF_GetBlock(fileDesc, HTindex, hashBlock));
        hashuTaburu = BF_Block_GetData(hashBlock);
        HTindex = ((HashTable*)hashuTaburu)->nextHT; // update HTindex
        BF_UnpinBlock(hashBlock);
    } while (HTindex != -1); // while more HTs

    char* data;
    BF_Block* bucketBlock;
    BF_Block_Init(&bucketBlock);

    for (int i = 0; i < num_of_blocks; i++) {
        if (!inLL(explorer, i)) //if not hash block
        {
            CALL_BF(BF_GetBlock(fileDesc, i, bucketBlock));
            data = BF_Block_GetData(bucketBlock);

            if (((Bucket*)data)->recordCount > max_records)
                max_records = ((Bucket*)data)->recordCount;

            if (((Bucket*)data)->recordCount < min_records)
                min_records = ((Bucket*)data)->recordCount;

            total_records += ((Bucket*)data)->recordCount;
            total_buckets++;
            BF_UnpinBlock(bucketBlock);
        }
    }
    freeLL(explorer);

    if (total_buckets!=0){
      // computing using records in buckets
      double average = total_records / total_buckets;

      if ((min_records == BF_BLOCK_SIZE / sizeof(Record)) || (max_records == 0))
          return HT_ERROR;

      // printing the statistics
      printf("Minimum Records: %d\n", min_records);
      printf("Average Records: %f\n", average);
      printf("Maximum Records: %d\n", max_records);

    }

    return HT_OK;
}