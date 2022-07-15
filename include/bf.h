#ifndef BF_H
#define BF_H

#ifdef __cplusplus
extern "C" {
#endif

#define BF_BLOCK_SIZE 512      
#define BF_BUFFER_SIZE 100     
#define BF_MAX_OPEN_FILES 100  

typedef enum BF_ErrorCode {
  BF_OK,
  BF_OPEN_FILES_LIMIT_ERROR,     
  BF_INVALID_FILE_ERROR,        
  BF_ACTIVE_ERROR,               
  BF_FILE_ALREADY_EXISTS,        
  BF_FULL_MEMORY_ERROR,          
  BF_INVALID_BLOCK_NUMBER_ERROR, 
  BF_AVAILABLE_PIN_BLOCKS_ERROR,
  BF_ERROR
} BF_ErrorCode;

typedef enum ReplacementAlgorithm {
  LRU,
  MRU
} ReplacementAlgorithm;


// Block struct
typedef struct BF_Block BF_Block;

/*
  * The BF_Block_Init function initializes and allocates the appropriate memory
  * for the BF_BLOCK structure.
 */
void BF_Block_Init(BF_Block **block);

/*
  * The BF_Block_Destroy function frees the memory it occupies
  * the BF_BLOCK structure.
 */
void BF_Block_Destroy(BF_Block **block);

/*
  * The BF_Block_SetDirty function changes the state of the block to dirty.
  * This practically means that the block data has been changed and the
  * BF level when needed will write the block back to disk. In
  * case where we simply read the data without changing it then
  * we don't need to call the function.
 */
void BF_Block_SetDirty(BF_Block *block);

/*
  * The BF_Βlock_GetData function returns a pointer to the Block's data.
  * If we change the data we should make the block dirty with the call
  * of the BF_Block_GetData function.
 */
char* BF_Block_GetData(const BF_Block *block);

/*
  * The BF_Init function initializes the BF layer.
  * We can choose between two Block replacement policies
  * that of LRU and that of MRU.
 */
BF_ErrorCode BF_Init(const ReplacementAlgorithm repl_alg);

/*
  * The BF_CreateFile function creates a file named filename
  * which consists of blocks. If the file already exists then it is returned
  * error code. In case of successful execution of the function it is returned
  * BF_OK, while on failure an error code is returned. If you want to
  * see the type of error you can call the BF_PrintError function.
 */
BF_ErrorCode BF_CreateFile(const char* filename);

/*
  * The BF_OpenFile function opens an existing file of named blocks
  * filename and returns the file identifier in the variable
  * file_desc. In case of success BF_OK is returned while in case
  * on failure, an error code is returned. If you want to see the species
  * of the error you can call the BF_PrintError function.
 */
BF_ErrorCode BF_OpenFile(const char* filename, int *file_desc);

/*
  * BF_CloseFile function closes open file with ID number
  * file_desc. In case of success BF_OK is returned while in case
  * on failure, an error code is returned. If you want to see it
  * kind of error you can call the function BF_PrintError.
 */
BF_ErrorCode BF_CloseFile(const int file_desc);

/*
  * The Get_BlockCounter function accepts the ID number as an argument
  * file_desc of an open file from block and finds the number of
  * of its available blocks, which it returns to the blocks_num variable.
  * In case of success, BF_OK is returned, while in case of failure,
  * an error code is returned. If you want to see the type of error
  * you can call the BF_PrintError function.
 */
BF_ErrorCode BF_GetBlockCounter(const int file_desc, int *blocks_num);

/*
 * With the function BF_AllocateBlock a new block is allocated for the
 * file with id number blockFile. The new block is always committed
 * at the end of the file, so the block number is
 * BF_getBlockCounter(file_desc) - 1. The block being bound is pinned
 * in memory (pin) and is returned to the block variable. When you don't
 * we need this block again then we need to update the level
 * block by calling the BF_UnpinBlock function. In case of success
 * BF_OK is returned and on failure, a code is returned
 * errors. If you want to see the type of error you can call her
 * function BF_PrintError.
 */
BF_ErrorCode BF_AllocateBlock(const int file_desc, BF_Block *block);


/*
  * The function BF_GetBlock finds the block with number block_num of the open
  * file file_desc and returns it to the block variable. The block that
  * is bound pinned to memory (pin). When we don't need this anymore
  * the block then we need to update the block level by calling the function
  * BF_UnpinBlock. In case of success BF_OK is returned while in case
  * on failure, an error code is returned. If you want to see its kind
  * errors you can call the BF_PrintError function.
 */
BF_ErrorCode BF_GetBlock(const int file_desc,
                         const int block_num,
                         BF_Block *block);

/*
  * The BF_UnpinBlock function unpins the block from the Block layer
  * which will write it to disk at some point. In case of success
  * BF_OK is returned and on failure, a code is returned
  * errors. If you want to see the type of error you can call her
  * function BF_PrintError.
 */
BF_ErrorCode BF_UnpinBlock(BF_Block *block);

/*
  * The BF_PrintError function helps to print the possible errors
  * exist by calling block file level functions. It is being printed
  * on stderr a description of the most error.
 */
void BF_PrintError(BF_ErrorCode err);

/*
  * The BF_Close function calls the Block layer by writing to disk whatever
  * block had in memory.
 */
BF_ErrorCode BF_Close();

#ifdef __cplusplus
}
#endif
#endif // BF_H
