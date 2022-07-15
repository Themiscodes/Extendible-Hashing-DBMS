#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> 
#include "bf.h"
#include "sht_file.h"
#define MAX_OPEN_FILES 20

#define CALL_BF(call)             \
    {                             \
        BF_ErrorCode code = call; \
        if (code != BF_OK) {      \
            BF_PrintError(code);  \
            return HT_ERROR;      \
        }                         \
    }

// the hash function for the strings of the secondary hashtable. adapted from Jenkins one at a time
int hashStringFunction(char* theStringInQuestion, int depth, int stringLength){ 

    // variables to be used
    int reversi = 0;
    int allbits = 32;
    int hash, i;
    
    // so that each character is used
    for (hash = i = 0; i < stringLength; ++i) {
        hash += theStringInQuestion[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    // reverse these bits
    int index = hash;
    while (allbits--) {
        reversi = (reversi << 1) | (index & 1);
        index >>= 1;
    }

    // get only as many bits as the secondary depth
    reversi = reversi >> (32 - depth);

    // finally reverse them
    int reversi2 = 0;
    while (depth--) {
        reversi2 = (reversi2 << 1) | (reversi & 1);
        reversi >>= 1;
    }
    
    return reversi2;
}

static Index sht_index; //same format as primary

HT_ErrorCode SHT_Init(){

    sht_index.fileCount = 0;
    for (int i = 0; i < MAX_OPEN_FILES; i++)
        sht_index.fileDesc[i] = -1;

    return HT_OK;
}

HT_ErrorCode SHT_CreateSecondaryIndex(const char* sfileName, char* attrName, int attrLength, int depth, char* fileName){
    
    // max open files check
    if (sht_index.fileCount == MAX_OPEN_FILES)
        return HT_ERROR;
    int fd1;
    BF_Block* hashBlock;
    BF_Block_Init(&hashBlock);
    CALL_BF(BF_CreateFile(sfileName));
    CALL_BF(BF_OpenFile(sfileName, &fd1));

    // initialise secondary hashtable information
    CALL_BF(BF_AllocateBlock(fd1, hashBlock));
    char* data = BF_Block_GetData(hashBlock);
    SHT _sht;
    _sht.depth = depth;
    _sht.nextHT = -1;
    strcpy(_sht.attrName, attrName);
    _sht.attrlen = attrLength;
    for (int i = 0; i < 64; i++) {
        _sht.buckets[i] = -1;
    }
    strcpy(_sht.primary_HT, fileName);

    // place the block in the file
    memcpy(data, &_sht, sizeof(SHT));

    BF_Block_SetDirty(hashBlock);
    CALL_BF(BF_UnpinBlock(hashBlock));
    CALL_BF(BF_CloseFile(fd1));

    return HT_OK;
}

HT_ErrorCode SHT_OpenSecondaryIndex(const char* sfileName, int* indexDesc){

    if (sht_index.fileCount == MAX_OPEN_FILES)
        return HT_ERROR;
    int fd;
    CALL_BF(BF_OpenFile(sfileName, &fd));
    
    // save the new file information in the indexTable
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (sht_index.fileDesc[i] == -1) {
            sht_index.fileDesc[i] = fd;
            sht_index.fileCount += 1;
            *indexDesc = i;
            return HT_OK;
        }
    }
    return HT_ERROR;
}

HT_ErrorCode SHT_CloseSecondaryIndex(int indexDesc){

    // close the file and update index table
    if ((indexDesc < MAX_OPEN_FILES) && (indexDesc > -1) && (sht_index.fileDesc[indexDesc] != -1)) {
        CALL_BF(BF_CloseFile(sht_index.fileDesc[indexDesc])); 
        sht_index.fileDesc[indexDesc] = -1;
        sht_index.fileCount -= 1;
        return HT_OK;
    }

    return HT_ERROR;
}

HT_ErrorCode SHT_SecondaryInsertEntry(int indexDesc, SecondaryRecord record){

    int fileDesc;
    if ((indexDesc < MAX_OPEN_FILES) && (indexDesc > -1) && (sht_index.fileDesc[indexDesc] != -1)) {
        fileDesc = sht_index.fileDesc[indexDesc];
    } else {
        return HT_ERROR;
    }

    BF_Block* hashBlock;
    BF_Block_Init(&hashBlock);
    CALL_BF(BF_GetBlock(fileDesc, 0, hashBlock));
    char* sht = BF_Block_GetData(hashBlock);

    int ldepth = ((SHT*)sht)->depth;

    // hash the key
    int where = hashStringFunction(record.index_key, ldepth, strlen(record.index_key));

    // find the position in the SHT following the model of the primary HT
    int whichHTblock = where / 64;
    for (int i = 0; i < whichHTblock; ++i) {
        int next = ((SHT*)sht)->nextHT;
        CALL_BF(BF_UnpinBlock(hashBlock));
        CALL_BF(BF_GetBlock(fileDesc, next, hashBlock));
        sht = BF_Block_GetData(hashBlock);
    }

    int bucket = ((SHT*)sht)->buckets[where % 64];

    if (bucket == -1) { // case that a new bucket is needed

        BF_Block* litoBucket;
        BF_Block_Init(&litoBucket);
        char* data;
        CALL_BF(BF_AllocateBlock(fileDesc, litoBucket));
        data = BF_Block_GetData(litoBucket);
        SecondaryBucket bucketino;
        bucketino.records[0] = record;
        bucketino.recordCount = 1;
        bucketino.localDepth = ((SHT*)sht)->depth;
        memcpy(data, &bucketino, sizeof(SecondaryBucket));

        BF_Block_SetDirty(litoBucket);
        CALL_BF(BF_UnpinBlock(litoBucket));

        int newBlockCounter;
        CALL_BF(BF_GetBlockCounter(fileDesc, &newBlockCounter));
        ((SHT*)sht)->buckets[where % 64] = newBlockCounter - 1;

        BF_Block_SetDirty(hashBlock);
        CALL_BF(BF_UnpinBlock(hashBlock));

        return HT_OK;
    } else { // the bucket already exists

        char* bucketData;
        BF_Block* bucketBlock;
        BF_Block_Init(&bucketBlock);
        CALL_BF(BF_GetBlock(fileDesc, bucket, bucketBlock));
        bucketData = BF_Block_GetData(bucketBlock);
        
        // if its full
        if (((SecondaryBucket*)bucketData)->recordCount == 21) {
            printf("The bucket is full.\n"); 
            return HT_ERROR;
        } else { 
            // if there is space place it in
            int newIndexofRec = ((SecondaryBucket*)bucketData)->recordCount;
            ((SecondaryBucket*)bucketData)->records[newIndexofRec] = record;
            ((Bucket*)bucketData)->recordCount += 1;
            BF_Block_SetDirty(bucketBlock);
            CALL_BF(BF_UnpinBlock(bucketBlock));
            BF_Block_SetDirty(hashBlock);
            CALL_BF(BF_UnpinBlock(hashBlock));
            return HT_OK;
        }
    }

    return HT_ERROR;
}

// this function works with either city or surname as key
HT_ErrorCode SHT_SecondaryUpdateEntry(int indexDesc, UpdateRecordArray* updateArray){
    
    int fileDesc;
    if ((indexDesc < MAX_OPEN_FILES) && (indexDesc > -1) && (sht_index.fileDesc[indexDesc] != -1)) {
        fileDesc = sht_index.fileDesc[indexDesc];
    } else {
        return HT_ERROR;
    }

    BF_Block* hashBlock;
    BF_Block_Init(&hashBlock);
    CALL_BF(BF_GetBlock(fileDesc, 0, hashBlock));
    char* sht = BF_Block_GetData(hashBlock);

    // secondary hash table depth
    int ldepth = ((SHT*)sht)->depth;

    if (strcmp(((SHT*)sht)->attrName,"city")==0){ // if the attribute is: city

        // FOR EVERY record that it's position has changed
        for (int i=0;i<updateArray->howManyUpdates;i++){
            int where = hashStringFunction((updateArray->updatedRecord[i]).city, ldepth, strlen((updateArray->updatedRecord[i]).city));
            int whichHTblock = where / 64;
            for (int i = 0; i < whichHTblock; ++i) {
                int next = ((SHT*)sht)->nextHT;
                CALL_BF(BF_UnpinBlock(hashBlock));
                CALL_BF(BF_GetBlock(fileDesc, next, hashBlock));
                sht = BF_Block_GetData(hashBlock);
            }
            
            // find bucket in the secondary
            int bucket = ((SHT*)sht)->buckets[where % 64];
            if (bucket==-1){
                BF_UnpinBlock(hashBlock);
                return HT_ERROR;
            }
            else{
                char* bucketData;
                BF_Block* bucketBlock;
                BF_Block_Init(&bucketBlock);
                CALL_BF(BF_GetBlock(fileDesc, bucket, bucketBlock));
                bucketData = BF_Block_GetData(bucketBlock);

                // FOR EVERY record of the bucket
                for (int jjj=0; jjj<((SecondaryBucket*)bucketData)->recordCount;jjj++){
                    // find the one has the old tupleID and the same city name
                    if ((strcmp(((SecondaryBucket*)bucketData)->records[jjj].index_key,updateArray->updatedRecord[i].city)==0)&&((SecondaryBucket*)bucketData)->records[jjj].tupleId== (updateArray->updatedRecord[i]).oldTupleId){
                        // update with the new tupleId
                        ((SecondaryBucket*)bucketData)->records[jjj].tupleId= updateArray->updatedRecord[i].newTupleId;
                    }
                }
                BF_Block_SetDirty(bucketBlock);
                CALL_BF(BF_UnpinBlock(bucketBlock));
            }

            BF_Block_SetDirty(hashBlock);
            CALL_BF(BF_UnpinBlock(hashBlock));
            CALL_BF(BF_GetBlock(fileDesc, 0, hashBlock)); // start over for the next record
            sht = BF_Block_GetData(hashBlock);

        }
    }
    else{ // the same for the surname
        
        // FOR EVERY record that it's position has changed
        for (int i=0;i<updateArray->howManyUpdates;i++){
            int where = hashStringFunction((updateArray->updatedRecord[i]).surname, ldepth, strlen((updateArray->updatedRecord[i]).surname));
            int whichHTblock = where / 64;
            for (int i = 0; i < whichHTblock; ++i) {
                int next = ((SHT*)sht)->nextHT;
                CALL_BF(BF_UnpinBlock(hashBlock));
                CALL_BF(BF_GetBlock(fileDesc, next, hashBlock));
                sht = BF_Block_GetData(hashBlock);
            }
            // find bucket in the secondary
            int bucket = ((SHT*)sht)->buckets[where % 64];
            if (bucket==-1){
                BF_UnpinBlock(hashBlock);
                return HT_ERROR;
            }
            else{
                char* bucketData;
                BF_Block* bucketBlock;
                BF_Block_Init(&bucketBlock);
                CALL_BF(BF_GetBlock(fileDesc, bucket, bucketBlock));
                bucketData = BF_Block_GetData(bucketBlock);
                // FOR EVERY record of the bucket
                for (int jjj=0; jjj<((SecondaryBucket*)bucketData)->recordCount;jjj++){
                    // find the one has the old tupleID and the same surname
                    if ((strcmp(((SecondaryBucket*)bucketData)->records[jjj].index_key,updateArray->updatedRecord[i].surname)==0)&&((SecondaryBucket*)bucketData)->records[jjj].tupleId== (updateArray->updatedRecord[i]).oldTupleId){
                        // update with the new tupleId
                        ((SecondaryBucket*)bucketData)->records[jjj].tupleId= updateArray->updatedRecord[i].newTupleId;
                    }
                }
                BF_Block_SetDirty(bucketBlock);
                CALL_BF(BF_UnpinBlock(bucketBlock));
            }

            BF_Block_SetDirty(hashBlock);
            CALL_BF(BF_UnpinBlock(hashBlock));
            CALL_BF(BF_GetBlock(fileDesc, 0, hashBlock)); // start over for the next record
            sht = BF_Block_GetData(hashBlock);

        }
    }
    
    updateArray->howManyUpdates=0; // "reset" it to 0 for the next time
    
    BF_Block_SetDirty(hashBlock);
    CALL_BF(BF_UnpinBlock(hashBlock));
    
    return HT_OK;
}

// search and print entries with index_key
HT_ErrorCode SHT_PrintAllEntries(int sindexDesc, char* index_key){

    int fileDesc;
    if ((sindexDesc < MAX_OPEN_FILES) && (sindexDesc > -1) && (sht_index.fileDesc[sindexDesc] != -1)) {
        fileDesc = sht_index.fileDesc[sindexDesc];
    } else
        return HT_ERROR;

    BF_Block* hashBlock;
    BF_Block_Init(&hashBlock);
    CALL_BF(BF_GetBlock(fileDesc, 0, hashBlock));
    char* sht = BF_Block_GetData(hashBlock);

    char primary_name[50]; //which primary hash table is attached
    strcpy(primary_name, ((SHT*)sht)->primary_HT);

    int primary_fd;
    CALL_BF(BF_OpenFile(primary_name, &primary_fd));

    // which block is the hashtable
    int where = hashStringFunction(index_key, ((SHT*)sht)->depth, strlen(index_key));
    int whichHT = where / 64;
    for (int i = 0; i < whichHT; i++) {
        CALL_BF(
            BF_GetBlock(fileDesc, ((SHT*)sht)->nextHT, hashBlock));
        sht = BF_Block_GetData(hashBlock);
    }
    int whichSHTslot = where % 64;
    int whichSHT_fblock = ((SHT*)sht)->buckets[whichSHTslot];

    if (whichSHT_fblock == -1) {
        // if the block doesn't exist
        printf("Invalid attribute\n");
        BF_UnpinBlock(hashBlock);
        return HT_OK;
    }
    BF_Block* bucket;
    BF_Block_Init(&bucket);
    CALL_BF(BF_GetBlock(fileDesc, whichSHT_fblock, bucket));

    char* data = BF_Block_GetData(bucket);

    BF_Block* main_bucket;
    BF_Block_Init(&main_bucket);

    // FOR EVERY record in the bucket
    for (int i = 0; i < ((SecondaryBucket*)data)->recordCount; i++) {
        SecondaryRecord r = ((SecondaryBucket*)data)->records[i];
        
        // if the index key is the same
        if (strcmp(r.index_key, index_key) == 0) {

            // go to the primary block that the tupleID points to
            int tupleID = r.tupleId;

            // divide by num_of_rec_in_block and subtract 1 for blockId
            int blockn = tupleID / 8 -1;
            CALL_BF(BF_GetBlock(primary_fd, blockn, main_bucket));

            // modulo num_of_rec_in_block to get index_of_rec_in_block
            int recordIndex = (tupleID % 8);
            char* primary_data = BF_Block_GetData(main_bucket);

            // print record
            Record primary_record = ((Bucket*)primary_data)->records[recordIndex];
            printf("ID: %d, name: %s, surname: %s, city: %s\n", primary_record.id, primary_record.name,
                primary_record.surname, primary_record.city);
            BF_UnpinBlock(main_bucket);
        }
    }

    BF_UnpinBlock(main_bucket);
    BF_UnpinBlock(bucket);
    BF_UnpinBlock(hashBlock);
  
    return HT_OK;
}

HT_ErrorCode SHT_HashStatistics(char* filename){

    int fileDesc, sindexDesc;
    SHT_OpenSecondaryIndex(filename, &sindexDesc);
    if ((sindexDesc < MAX_OPEN_FILES) && (sindexDesc > -1) && (sht_index.fileDesc[sindexDesc] != -1)) {
        fileDesc = sht_index.fileDesc[sindexDesc];
    } else
        return HT_ERROR;

    // computing total blocks in the file
    int num_of_blocks;
    CALL_BF(BF_GetBlockCounter(fileDesc, &num_of_blocks));
    printf("File '%s' has %d Blocks\n", filename, num_of_blocks);
    if(num_of_blocks == 1){
        printf("No data yet in the file!\n");
        return HT_OK;
    }

    int total_buckets = 0; // counter buckets
    int total_records = 0; // counter for records
    int min_records = 22; 
    int max_records = 0;  

    char* sht;
    BF_Block* sHashBlock;
    BF_Block_Init(&sHashBlock);

    int SHTindex = 0;
    LL* explorer = NULL; // traverse blocks

    do {
        insertLL(SHTindex, &explorer); // get which blocks are hashblocks
        CALL_BF(BF_GetBlock(fileDesc, SHTindex, sHashBlock));
        sht = BF_Block_GetData(sHashBlock);
        SHTindex = ((SHT*)sht)->nextHT; // the next SHTindex
        BF_UnpinBlock(sHashBlock);
    } while (SHTindex != -1); // while there are more

    char* data;
    BF_Block* sBucket;
    BF_Block_Init(&sBucket);

    for (int i = 0; i < num_of_blocks; i++) {
        if (!inLL(explorer, i)) // if not secondary hash block
        {
            CALL_BF(BF_GetBlock(fileDesc, i, sBucket));
            data = BF_Block_GetData(sBucket);

            if (((SecondaryBucket*)data)->recordCount > max_records)
                max_records = ((SecondaryBucket*)data)->recordCount;

            if (((SecondaryBucket*)data)->recordCount < min_records)
                min_records = ((SecondaryBucket*)data)->recordCount;

            total_records += ((SecondaryBucket*)data)->recordCount;
            total_buckets++;
            BF_UnpinBlock(sBucket);
        }
    }
    freeLL(explorer);

    if (total_buckets!=0){

      // compute using total records by buckets
      double average = total_records / total_buckets;

      if ((min_records == 22) || (max_records == 0))
          return HT_ERROR;

      // print the statistics
      printf("Minimum Records: %d\n", min_records);
      printf("Average Records: %f\n", average);
      printf("Maximum Records: %d\n", max_records);

    }

    return HT_OK;
}

// union in the index key
HT_ErrorCode SHT_InnerJoin(int sindexDesc1, int sindexDesc2, char* index_key){
    
    // initial error checks
    int fd1, fd2;
    if ((sindexDesc1 < MAX_OPEN_FILES) && (sindexDesc1 > -1) && (sht_index.fileDesc[sindexDesc1] != -1)) {
        fd1 = sht_index.fileDesc[sindexDesc1];
    } else
        return HT_ERROR;

    if ((sindexDesc2 < MAX_OPEN_FILES) && (sindexDesc2 > -1) && (sht_index.fileDesc[sindexDesc2] != -1)) {
        fd2 = sht_index.fileDesc[sindexDesc2];
    } else
        return HT_ERROR;

    // get at sht1 the secondary hash table of sindexDesc1
    BF_Block* hashBlock1;
    BF_Block_Init(&hashBlock1);
    CALL_BF(BF_GetBlock(fd1, 0, hashBlock1));
    char* sht1 = BF_Block_GetData(hashBlock1);
    char primary_name1[50];
    strcpy(primary_name1, ((SHT*)sht1)->primary_HT);

    // fd of its primary
    int primary_fd1;
    CALL_BF(BF_OpenFile(primary_name1, &primary_fd1));
    
    // get at sht2 the secondary hash table of sindexDesc2
    BF_Block* hashBlock2;
    BF_Block_Init(&hashBlock2);
    CALL_BF(BF_GetBlock(fd2, 0, hashBlock2));
    char* sht2 = BF_Block_GetData(hashBlock2);
    char primary_name2[50];
    strcpy(primary_name2, ((SHT*)sht2)->primary_HT);

    // fd of its primary
    int primary_fd2;
    CALL_BF(BF_OpenFile(primary_name2, &primary_fd2));

    // check if the secondaries are hashing with the same attribute
    if (strcmp(((SHT*)sht1)->attrName,((SHT*)sht2)->attrName)!=0){
        printf("Invalid attribute names\n");
        BF_UnpinBlock(hashBlock1);
        BF_UnpinBlock(hashBlock2);
        return HT_OK;
    }

    // case of join at a key
    if (index_key != NULL) {

        // find the buckets in the secondary hash tables
        int where1 = hashStringFunction(index_key, ((SHT*)sht1)->depth, strlen(index_key));
        int where2 = hashStringFunction(index_key, ((SHT*)sht2)->depth, strlen(index_key));
        int whichHT1 = where1 / 64;
        int HTindex = ((SHT*)sht1)->nextHT;
        for (int i = 0; i < whichHT1; i++) {
            BF_UnpinBlock(hashBlock1);
            CALL_BF(BF_GetBlock(fd1, HTindex, hashBlock1));
            sht1 = BF_Block_GetData(hashBlock1);
            HTindex = ((SHT*)sht1)->nextHT;
        }

        int whichSHTslot1 = where1 % 64;
        int whichSHT_fblock1 = ((SHT*)sht1)->buckets[whichSHTslot1];

        if (whichSHT_fblock1 == -1) {
            printf("Invalid attribute\n");
            BF_UnpinBlock(hashBlock1);
            BF_UnpinBlock(hashBlock2);
            return HT_OK;
        }

        BF_Block* bucket1; //sht1 bucket
        BF_Block_Init(&bucket1);
        CALL_BF(BF_GetBlock(fd1, whichSHT_fblock1, bucket1));

        char* data1 = BF_Block_GetData(bucket1); //bucket of first secondary hash table

        int whichHT2 = where2 / 64;
        HTindex = ((SHT*)sht2)->nextHT;
        for (int i = 0; i < whichHT2; i++) {
            BF_UnpinBlock(hashBlock2);
            CALL_BF(BF_GetBlock(fd1, HTindex, hashBlock2));
            sht2 = BF_Block_GetData(hashBlock2);
            HTindex = ((SHT*)sht2)->nextHT;
        }
        int whichSHTslot2 = where2 % 64;
        int whichSHT_fblock2 = ((SHT*)sht2)->buckets[whichSHTslot2];

        if (whichSHT_fblock2 == -1) {
            printf("Invalid attribute\n");
            BF_UnpinBlock(hashBlock1);
            BF_UnpinBlock(hashBlock2);
            BF_UnpinBlock(bucket1);
            return HT_OK;
        }

        BF_Block* bucket2; //sht2 bucket
        BF_Block_Init(&bucket2);
        CALL_BF(BF_GetBlock(fd2, whichSHT_fblock2, bucket2));

        BF_Block *main_bucket, *main_bucket2;
        BF_Block_Init(&main_bucket); // storing the content of the bucket of the primary hash table 1
        BF_Block_Init(&main_bucket2); // storing the content of the bucket of the primary hash table 2

        char* data2 = BF_Block_GetData(bucket2);

        // FOR EVERY record of the bucket of the first SHT that the key is hashing to
        for (int i = 0; i < ((SecondaryBucket*)data1)->recordCount; i++) {
            SecondaryRecord r1 = ((SecondaryBucket*)data1)->records[i];

            // IF it contains this key
            if (strcmp(r1.index_key, index_key) == 0) {
                int tupleID = r1.tupleId;
                int blockn = tupleID / 8 - 1;
                CALL_BF(BF_GetBlock(primary_fd1, blockn, main_bucket));
                int recordIndex = tupleID % 8;
                char* primary_data = BF_Block_GetData(main_bucket);

                // get the record from the primary
                Record primary_record1 = ((Bucket*)primary_data)->records[recordIndex];

                // FOR EVERY record of the bucket of the second SHT that the key is hashing to
                for (int j = 0; j < ((SecondaryBucket*)data2)->recordCount; j++) {
                    SecondaryRecord r2 = ((SecondaryBucket*)data2)->records[j];

                    // IF it contains this key
                    if (strcmp(r2.index_key, r1.index_key) == 0) {
                        int tupleID2 = r2.tupleId;
                        int blockn2 = tupleID2 / 8 - 1;
                        CALL_BF(BF_GetBlock(primary_fd2, blockn2, main_bucket2));
                        int recordIndex2 = tupleID2 % 8;
                        char* primary_data2 = BF_Block_GetData(main_bucket2);

                        // get the record from its primary as well
                        Record primary_record2 = ((Bucket*)primary_data2)->records[recordIndex2];

                        // print the Inner Join of them in one line
                        printf("ID1: %d, name1: %s, surname1: %s, city1: %s | ID2: %d, name2: %s, surname2: %s, city2: %s \n", primary_record1.id, primary_record1.name,
                            primary_record1.surname, primary_record1.city, primary_record2.id, primary_record2.name,
                            primary_record2.surname, primary_record2.city);
                        BF_UnpinBlock(main_bucket2);
                    }
                }
                BF_UnpinBlock(main_bucket);
            }
        }
        BF_UnpinBlock(main_bucket2);
        BF_UnpinBlock(main_bucket);
        BF_UnpinBlock(bucket1);
        BF_UnpinBlock(bucket2);

    } 
    else { // NULL case
        
        BF_Block *main_bucket1, *main_bucket2;
        BF_Block_Init(&main_bucket1);
        BF_Block_Init(&main_bucket2);
        int HTindex=0;
        do {
            
            int htsize = (int)pow(2.0, (double)(((SHT*)sht1)->depth));
            int parseThrough = htsize > 64 ? 64 : htsize;

            // FOR EVERY BLOCK of the first SHT
            for(int opa=0;opa<parseThrough;opa++){

                // READ the blockID
                int blockID = ((SHT*)sht1)->buckets[opa];
                if (blockID!=-1){ 

                    // READ the bucket
                    char *theBucket1;
                    BF_Block *bucketino;
                    BF_Block_Init(&bucketino);
                    CALL_BF(BF_GetBlock(fd1, blockID, bucketino));
                    theBucket1 = BF_Block_GetData(bucketino);

                    // FOR EVERY bucket tuple
                    for (int v =0;v<((SecondaryBucket*)theBucket1)->recordCount;v++){
                        
                        // READ the key
                        char* firstKEY = ((SecondaryBucket*)theBucket1)->records[v].index_key;
 
                        // FIND the equivalent bucket in the second secondary
                        int myposinSHT2 = hashStringFunction(firstKEY, ((SHT*)sht2)->depth, strlen(firstKEY));
                        int whichHT2 = myposinSHT2 / 64;
                        int HTindex2 = ((SHT*)sht2)->nextHT;
                        for (int igf = 0; igf < whichHT2; igf++) {
                            BF_UnpinBlock(hashBlock2);
                            CALL_BF(BF_GetBlock(fd2, HTindex2, hashBlock2));
                            sht2 = BF_Block_GetData(hashBlock2);
                            HTindex2 = ((SHT*)sht2)->nextHT;
                        }
                        int blockID2 = ((SHT*)sht2)->buckets[myposinSHT2 % 64];

                        // if it's not empty
                        if (blockID2 != -1) {

                            // READ the bucket
                            char *theBucket2;
                            BF_Block *bucketino2;
                            BF_Block_Init(&bucketino2);
                            CALL_BF(BF_GetBlock(fd2, blockID2, bucketino2));
                            theBucket2 = BF_Block_GetData(bucketino2);

                            // FOR EVERY bucket tuple
                            for (int ffs =0;ffs<((SecondaryBucket*)theBucket2)->recordCount;ffs++){

                                // IF SHT1.key == SHT2.key
                                if (strcmp(firstKEY,  ((SecondaryBucket*)theBucket2)->records[ffs].index_key)==0){

                                    char* main_bucket1_data;
                                    char* main_bucket2_data;

                                    int myblockID1 = (((SecondaryBucket*)theBucket1)->records[v].tupleId)/8-1;
                                    int myblockID2 = (((SecondaryBucket*)theBucket2)->records[ffs].tupleId)/8-1;

                                    // get the record from the primary 1
                                    CALL_BF(BF_GetBlock(primary_fd1, myblockID1, main_bucket1));
                                    main_bucket1_data=BF_Block_GetData(main_bucket1);

                                    // get the record from the primary 2
                                    CALL_BF(BF_GetBlock(primary_fd2, myblockID2, main_bucket2));
                                    main_bucket2_data=BF_Block_GetData(main_bucket2);

                                    Record one = ((Bucket*)main_bucket1_data)->records[(((SecondaryBucket*)theBucket1)->records[v].tupleId)%8];
                                    Record two = ((Bucket*)main_bucket2_data)->records[(((SecondaryBucket*)theBucket2)->records[ffs].tupleId)%8];

                                    // print the inner join of them in one line
                                    printf("ID1: %d, name1: %s, surname1: %s, city1: %s | ID2: %d, name2: %s, surname2: %s, city2: %s \n", one.id, one.name, one.surname, one.city, two.id, two.name, two.surname, two.city);
                                    
                                    BF_UnpinBlock(main_bucket2);
                                    BF_UnpinBlock(main_bucket1);

                                }

                            }
        
                            BF_UnpinBlock(bucketino2);
                        }

                        // reset the sht2 for the next parsing
                        BF_UnpinBlock(hashBlock2);
                        CALL_BF(BF_GetBlock(fd2, 0, hashBlock2));
                        sht2 = BF_Block_GetData(hashBlock2);
                    }

                    BF_UnpinBlock(bucketino);
                }

            }

            HTindex = ((SHT*)sht1)->nextHT;
            BF_UnpinBlock(hashBlock1);
            if (HTindex!=-1){
                CALL_BF(BF_GetBlock(fd1, HTindex, hashBlock1));
                sht1 = BF_Block_GetData(hashBlock1);
            }
        }while(HTindex!=-1);

        BF_UnpinBlock(main_bucket2);
        BF_UnpinBlock(main_bucket1);
    }
    
    BF_UnpinBlock(hashBlock1);
    BF_UnpinBlock(hashBlock2);
    return HT_OK;
}