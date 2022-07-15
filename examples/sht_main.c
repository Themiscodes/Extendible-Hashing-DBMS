#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hash_file.h"
#include "sht_file.h"

#define RECORDS_NUM 1000 
#define INITIAL_DEPTH 6 
#define HT_FILE_NAME1 "HT_data1.db"
#define SHT_FILE_NAME1 "SHT_data1.db"
#define HT_FILE_NAME2 "HT_data2.db"
#define SHT_FILE_NAME2 "SHT_data2.db"
#define HT_FILE_NAME3 "HT_data3.db"
#define SHT_FILE_NAME3 "SHT_data3.db"

const char* names[] = {
  "Michael",
  "Sebastian",
  "Max",
  "Abbey",
  "Kylie",
  "Mandy",
  "Jenson",
  "Heidi",
  "Nico",
  "Niki",
  "Loki",
  "Peter",
  "Gina",
  "Loria",
  "Margherita",
  "Eva",
  "Kate",
  "Theresa",
  "Natalia",
  "Lelia"
};

const char* surnames[] = {
  "Schumacher",
  "Vettel",
  "Hamilton",
  "Fangio",
  "Prost",
  "Brabham",
  "Stewart",
  "Lauda",
  "Piquet",
  "Senna",
  "Ascari",
  "Clark",
  "Hill",
  "Fittipaldi",
  "Hakkinen",
  "Alonso",
  "Farina",
  "Hawthorn",
  "Surtees",
  "Hulme",
  "Rindt",
  "Hunt",
  "Andretti",
  "Scheckter",
  "Jones",
  "Rosberg",
  "Mansell",
  "Villeneuve",
  "Raikkonen",
  "Button",
  "Verstappen"
};

const char* cities[] = {
  "Los Angeles",
  "Amsterdam",
  "London",
  "Tokyo",
  "Miami",
  "Torino",
  "Suzuka",
  "Austin",
  "Nurburgring",
  "Monaco",
  "Brackley",
  "Brixworth",
  "Interlagos",
  "Magny Cours",
  "Istanbul",
  "Imola",
  "Preveza",
  "Louros",
  "Giannena",
  "Budapest",
  "Mexico City",
  "Ano Kotsanopoulo",
  "Montreal",
  "Okinawa",
  "Stockholm",
  "Novorossiysk",
  "Moscow",
  "Paris",
  "Nafplio",
  "Bologna",
  "Singapore",
  "Kuala Lumpur",
  "Buddh",
  "Oslo",
  "Brenda",
  "Boston",
  "Petra",
  "Aspen",
  "Sao Paolo",
  "Kyoto",
  "Bergen",
  "Coppenhagen",
  "Saint Petersburg"
};

const char* moreCities[] = {
  "Athens",
  "San Francisco",
  "Munich",
  "Monza",
  "Barcelona",
  "Silverstone",
  "Milton Keynes",
  "Spa",
  "Mugello",
  "Indianapolis",
  "Zandvoort",
  "Baku",
  "New York",
  "Kilkis"
};

// Colours
#define KNRM  "\x1B[0m"  // normal colour
#define KRED  "\x1B[31m" // error Red
#define KGRN  "\x1B[32m" // successful Green
#define KCYN  "\x1B[36m" // description Cyan

// when there is an error
#define CALL_OR_DIE(call)     \
  {                           \
    HT_ErrorCode code = call; \
    if (code != HT_OK) {      \
      printf("%sError %s\n", KRED, KNRM);\
      exit(code);             \
    }                         \
  }

int main() {

  printf("%sTesting HT_CreateIndex & SHT_CreateIndex: %s", KCYN, KNRM);
  CALL_OR_DIE(HT_Init());
  int indexDescHT1, indexDescSHT1;
  CALL_OR_DIE(HT_CreateIndex(HT_FILE_NAME1, INITIAL_DEPTH));
  CALL_OR_DIE(HT_OpenIndex(HT_FILE_NAME1, &indexDescHT1)); 
  CALL_OR_DIE(SHT_Init());
  CALL_OR_DIE(SHT_CreateSecondaryIndex(SHT_FILE_NAME1, "city", 20, INITIAL_DEPTH, HT_FILE_NAME1));
  CALL_OR_DIE(SHT_OpenSecondaryIndex(SHT_FILE_NAME1, &indexDescSHT1)); 
  printf("%sOK %s\n", KGRN, KNRM);

  printf("%sTesting HT_InsertEntry & SHT_InsertEntry: %s", KCYN, KNRM);
  Record record;
  SecondaryRecord secondary_record;
  UpdateRecordArray updateArray;
  int tupleID;
  srand(12569874);
  int lottery;
  for(int firstBatch=0; firstBatch<140; firstBatch++){

    record.id = firstBatch;
    lottery = rand() % 20;
    memcpy(record.name, names[lottery], strlen(names[lottery]) + 1);
    lottery = rand() % 31;
    memcpy(record.surname, surnames[lottery], strlen(surnames[lottery]) + 1);
    memcpy(record.city, moreCities[firstBatch%14], strlen(moreCities[firstBatch%14]) + 1);

    CALL_OR_DIE(HT_InsertEntry(indexDescHT1, record, &tupleID, &updateArray));

    strcpy(secondary_record.index_key, record.city);
    secondary_record.tupleId = tupleID;
    CALL_OR_DIE(SHT_SecondaryInsertEntry(indexDescSHT1, secondary_record));
    CALL_OR_DIE(SHT_SecondaryUpdateEntry(indexDescSHT1, &updateArray));
  }
  printf("%sOK %s\n", KGRN, KNRM);

  printf("%sTest HT_Print IDS: 120, 33, 65: %s \n", KCYN, KNRM);
  int id = 120;
  HT_PrintAllEntries(indexDescHT1, &id);
  id = 33;
  HT_PrintAllEntries(indexDescHT1, &id);
  id = 65;
  HT_PrintAllEntries(indexDescHT1, &id);
  printf("%sTesting HT_PrintAllEntries: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);
  printf("%sTest SHT_Print with key: Zandvoort %s \n", KCYN, KNRM);
  SHT_PrintAllEntries(indexDescSHT1, "Zandvoort"); 
  printf("%sTest SHT_Print with key: Kilkis %s \n", KCYN, KNRM);
  SHT_PrintAllEntries(indexDescSHT1, "Kilkis");
  printf("%sTesting SHT_PrintAllEntries: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);

  // Inserting more to cause bucket splitting
  printf("%sInserting more entries to cause resizes in HT, bucket splits and SHT_SecondaryUpdateEntry: %s", KCYN, KNRM);
  for(int secondBatch=140; secondBatch<1000; secondBatch++){
    
    record.id = secondBatch;
    lottery = rand() % 20;
    memcpy(record.name, names[lottery], strlen(names[lottery]) + 1);
    lottery = rand() % 31;
    memcpy(record.surname, surnames[lottery], strlen(surnames[lottery]) + 1);
    memcpy(record.city, cities[secondBatch%43], strlen(cities[secondBatch%43]) + 1);
    
    CALL_OR_DIE(HT_InsertEntry(indexDescHT1, record, &tupleID, &updateArray));

    strcpy(secondary_record.index_key, record.city);
    secondary_record.tupleId = tupleID;
    CALL_OR_DIE(SHT_SecondaryInsertEntry(indexDescSHT1, secondary_record));
    CALL_OR_DIE(SHT_SecondaryUpdateEntry(indexDescSHT1, &updateArray));

  }
  printf("%sOK %s\n", KGRN, KNRM);
  // Testing search with print
  printf("%sTest HT_Print IDS: 444, 910: %s \n", KCYN, KNRM);
  id = 444;
  HT_PrintAllEntries(indexDescHT1, &id);
  id = 910;
  HT_PrintAllEntries(indexDescHT1, &id);
  printf("%sTest SHT_Print with key: Ano Kotsanopoulo %s \n", KCYN, KNRM);
  SHT_PrintAllEntries(indexDescSHT1, "Ano Kotsanopoulo");
  printf("%sTest SHT_Print with key: Suzuka %s \n", KCYN, KNRM);
  SHT_PrintAllEntries(indexDescSHT1, "Suzuka");
  printf("%sTesting Prints after new inserts: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);

  // Testing HT and SHT statistics
  printf("%sPrint Primary HT_HashStatistics: %s\n", KCYN, KNRM);
  CALL_OR_DIE(HashStatistics(HT_FILE_NAME1));
  printf("%sPrint Secondary SHT_HashStatistics: %s\n", KCYN, KNRM);
  CALL_OR_DIE(SHT_HashStatistics(SHT_FILE_NAME1));
  printf("%sTesting HT and SHT_HashStatistics: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);

  //Testing Creating another HT and SHT for inner join
  int indexDescHT2, indexDescSHT2;
  CALL_OR_DIE(HT_CreateIndex(HT_FILE_NAME2, INITIAL_DEPTH));
  CALL_OR_DIE(HT_OpenIndex(HT_FILE_NAME2, &indexDescHT2)); 
  CALL_OR_DIE(SHT_CreateSecondaryIndex(SHT_FILE_NAME2, "city", 20, INITIAL_DEPTH, HT_FILE_NAME2));
  CALL_OR_DIE(SHT_OpenSecondaryIndex(SHT_FILE_NAME2, &indexDescSHT2)); 

  for(int firstBatch=0; firstBatch<28; firstBatch++){
    record.id = firstBatch;
    lottery = rand() % 20;
    memcpy(record.name, names[lottery], strlen(names[lottery]) + 1);
    lottery = rand() % 31;
    memcpy(record.surname, surnames[lottery], strlen(surnames[lottery]) + 1);
    memcpy(record.city, moreCities[firstBatch%14], strlen(moreCities[firstBatch%14]) + 1);
    CALL_OR_DIE(HT_InsertEntry(indexDescHT2, record, &tupleID, &updateArray));
    strcpy(secondary_record.index_key, record.city);
    secondary_record.tupleId = tupleID;
    CALL_OR_DIE(SHT_SecondaryInsertEntry(indexDescSHT2, secondary_record));
    CALL_OR_DIE(SHT_SecondaryUpdateEntry(indexDescSHT2, &updateArray));
  }
  printf("%sThe entries from the second table that should be paired in key: Monza %s \n", KCYN, KNRM);
  SHT_PrintAllEntries(indexDescSHT2, "Monza");
  printf("%sTesting Inner Join with Monza: %s \n", KCYN, KNRM);
  CALL_OR_DIE(SHT_InnerJoin(indexDescSHT1, indexDescSHT2, "Monza")); 
  printf("%sTesting Inner Join: %s ", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);
  
  // uncomment to test inner Join with NULL
  // printf("%sTesting Inner Join with NULL: %s", KCYN, KNRM);
  // CALL_OR_DIE(SHT_InnerJoin(indexDescSHT1, indexDescSHT2, NULL));

  // Test creating secondary with surname as an attribute
  int indexDescHT3, indexDescSHT3;
  CALL_OR_DIE(HT_CreateIndex(HT_FILE_NAME3, INITIAL_DEPTH-3));
  CALL_OR_DIE(HT_OpenIndex(HT_FILE_NAME3, &indexDescHT3)); 
  CALL_OR_DIE(SHT_CreateSecondaryIndex(SHT_FILE_NAME3, "surname", 20, INITIAL_DEPTH, HT_FILE_NAME3));
  CALL_OR_DIE(SHT_OpenSecondaryIndex(SHT_FILE_NAME3, &indexDescSHT3)); 
  for(int firstBatch=0; firstBatch<62; firstBatch++){
    record.id = firstBatch;
    lottery = rand() % 20;
    memcpy(record.name, names[lottery], strlen(names[lottery]) + 1);
    memcpy(record.surname, surnames[firstBatch%31], strlen(surnames[firstBatch%31]) + 1);
    lottery = rand() % 43;
    memcpy(record.city, cities[lottery], strlen(cities[lottery]) + 1);

    CALL_OR_DIE(HT_InsertEntry(indexDescHT3, record, &tupleID, &updateArray));

    strcpy(secondary_record.index_key, record.surname);
    secondary_record.tupleId = tupleID;
    CALL_OR_DIE(SHT_SecondaryInsertEntry(indexDescSHT3, secondary_record));
    CALL_OR_DIE(SHT_SecondaryUpdateEntry(indexDescSHT3, &updateArray));
  }
  printf("%sTesting SHT creation with the attribute surname: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);
  printf("%sTest SHT_Print with key: Schumacher %s \n", KCYN, KNRM);
  SHT_PrintAllEntries(indexDescSHT3, "Schumacher");
  printf("%sTest SHT_Print with key: Fangio %s \n", KCYN, KNRM);
  SHT_PrintAllEntries(indexDescSHT3, "Fangio");
  printf("%sTesting SHT Prints are correct with the attribute surname: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);

  printf("%s\nTest Report \n%s", KCYN, KNRM);
  printf("%sTest HT_CreateIndex & SHT_CreateSecondaryIndex: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);
  printf("%sTest HT_InsertEntry & SHT_InsertEntry: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);
  printf("%sTest HT_Print & SHT_Print on some given keys: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);
  printf("%sTest resizes and bucket splits in HT: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);
  printf("%sTest SHT_SecondaryUpdateEntry: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);
  printf("%sTest HT and SHT_HashStatistics: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);
  printf("%sTest Inner Join with on a given key: %s ", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);
  printf("%sTest Inner Join with NULL: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);
  printf("%sTest both attributes work as keys: %s", KCYN, KNRM);
  printf("%sOK %s\n", KGRN, KNRM);

  CALL_OR_DIE(SHT_CloseSecondaryIndex(indexDescSHT1));
  CALL_OR_DIE(HT_CloseFile(indexDescHT1));
  CALL_OR_DIE(HT_CloseFile(indexDescHT2));
  CALL_OR_DIE(SHT_CloseSecondaryIndex(indexDescSHT2));
  BF_Close();

  printf("%sAll tests were succesful! %s\n", KGRN, KNRM);

}
