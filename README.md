## Extendible Hashing

The purpose of this project is to grasp the basic concepts of Database Management Systems and the improvement in performance Hash Tables can bring. Extendible Hashing is a dynamic hashing method wherein blocks and buckets are used to hash data. It is a flexible method in which the hash function also experiences changes. Furthermore, when the size of the Hash Table is doubled by using the buddy allocation system, there is no need for rehashing.

The functions of `hash_file.c` are used to create and process a file that keeps Records using Extendible Hashing. The functions of `sht_file.c` add the support for Secondary Hash Table files that hash on a desired attribute of the Records of the Primary. They also add the functionality of Inner Join, which can be performed in O(m+n) time (where m, n the bucket sizes), since searching for a key costs O(1) time. Both Primary and Secondary Hash Tables are block level implementations using the `libbf.so` library. 

The block level (BF) is a memory manager that works like cache between the disk and memory. Every time a block is requested, the BF level first examines if the block exists in memory, otherwise it reads it from the disk. Because the BF level doesn't have infinite memory at some point it will need to "throw" a block from the memory and bring another one to his position. As a policy I thought it wise to select LRU, "sacrificing" the least recently used block.


### Implementation

- In [hash_file.c](src/hash_file.c) is the implementation of the Primary Hash Table, while in [sht_file.c](src/sht_file.c) the implementation of the Secondary Hash Table. In the corresponding header files aside from the function declarations, are the Hash Table, Bucket and other useful struct declarations to keep the necessary level of abstraction.

- The buffer size is 512, so each bucket holds up to 8 records, since that record count and local depth is saved there as well. Each bucket is saved in one BF block so that there isn't compartmentalisation when it is stored in the buffer. The HashTable struct also fits in one BF block. In it aside from the depth and an array of bucket pointers, there is also nextHT, which is the block number of the file of the next piece of the Hash Table. 

- A bucket is created only when a record is about to be inserted in it. This way there isn't any unnecessary space in the file. The structs of the secondary Hash Table are similar to that of the primary, but since the SecondaryRecord holds only a tuple ID and a string that is twenty characters long, a bucket can fit 21 entries instead of 8.

- In HT_InsertEntry aside from the trivial case that a record fits in the bucket, there is also the case of bucket splitting (when the number of elements in a bucket exceed its size and the bucket is split into two parts) and of doubling the hash table size (when a bucket overflows and the local depth of the overflowing bucket is equal to the global depth).

- The UpdateRecordArray struct is used to monitor these changes, by keeping the new tuple ID, as well as the old. This way, if there are secondary Hash Tables they can be updated with this new information. The SHT_SecondaryUpdateEntry function makes those updates based on which attribute is being used for the hashing.

- As a hash function for the secondary Hash Table I used a variation of Jenkins' one at a time, which performs well on strings. After it has finished the process is similar to the hash function of the primary Hash Table so that it falls within the margin of the depth.

- Inner Join is implemented by using initially the secondary Hash Tables, to locate the tuple IDs based on the key. Then by having the block number of the Records, the information can be retrieved from the primary Hash Table and printed in one line.


### Compilation & Testing

In `bf_main.c` and `ht_main.c` are a few basic tests for the BF and the primary Hash Table functions, but the major unit testing for all parts is in [sht_main.c](examples/sht_main.c).

To compile run: `$ make`

To run the unit tests: `$ ./test`

To remove the object, executables and .db files that were created: `$ make clean`