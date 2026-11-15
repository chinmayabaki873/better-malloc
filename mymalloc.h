#ifndef MYMALLOC_H
#define MYMALLOC_H

#include <stddef.h>

#define FIRST_FIT 0
#define BEST_FIT  1
#define WORST_FIT 2
#define NEXT_FIT  3

#define HEAP_SIZE_1MB (1024 * 1024) // 1,048,576 bytes

// 24-byte Block Header (matches screenshot)
typedef struct Block {
    size_t size;          // 8 bytes: Usable data size
    int is_free;          // 8 bytes (int + 4B padding): 1 = true, 0 = false
    struct Block *next;   // 8 bytes: Pointer to next block in C heap
} Block;

int   Mem_Init(size_t size_of_region);
void *Mem_Alloc(size_t size);
int   Mem_Free(void *ptr);
int   Mem_FreeByIndex(int index);
void  Mem_SetPolicy(int policy);
void  Mem_Dump(void);
void  Mem_Reset(void);
int   Mem_ExportJSON(char *buf, size_t buf_size);

#endif