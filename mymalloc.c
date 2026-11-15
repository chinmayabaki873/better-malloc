#include "mymalloc.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

static Block *heap_head = NULL;
static Block *next_fit_rover = NULL;
static int current_policy = FIRST_FIT;
static int is_initialized = 0;
static size_t mapped_total_size = 0;

static size_t align8(size_t size) {
    return (size + 7) & ~((size_t)7);
}

void Mem_SetPolicy(int policy) {
    if (policy >= FIRST_FIT && policy <= NEXT_FIT) {
        current_policy = policy;
    }
}

int Mem_Init(size_t size_of_region) {
    if (is_initialized || size_of_region == 0) {
        return -1;
    }

    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size_t total_size = size_of_region;
    if (total_size % page_size != 0) {
        total_size += page_size - (total_size % page_size);
    }

    // Request 1 MB from OS using mmap ONCE
    void *mem = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        return -1;
    }

    mapped_total_size = total_size;
    heap_head = (Block *)mem;
    heap_head->size = total_size - sizeof(Block);
    heap_head->is_free = 1;
    heap_head->next = NULL;

    next_fit_rover = heap_head;
    is_initialized = 1;
    return 0;
}

static void split_block(Block *block, size_t req_size) {
    if (block->size >= req_size + sizeof(Block) + 8) {
        Block *new_block = (Block *)((char *)(block + 1) + req_size);
        new_block->size = block->size - req_size - sizeof(Block);
        new_block->is_free = 1;
        new_block->next = block->next;

        block->next = new_block;
        block->size = req_size;
    }
}

static void coalesce_all(void) {
    Block *curr = heap_head;
    while (curr != NULL && curr->next != NULL) {
        if (curr->is_free && curr->next->is_free) {
            if (next_fit_rover == curr->next) {
                next_fit_rover = curr;
            }
            curr->size += sizeof(Block) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void *Mem_Alloc(size_t size) {
    if (!is_initialized || size == 0) return NULL;

    size_t req_size = align8(size);
    Block *chosen = NULL;
    Block *curr = heap_head;

    if (current_policy == FIRST_FIT) {
        while (curr != NULL) {
            if (curr->is_free && curr->size >= req_size) { chosen = curr; break; }
            curr = curr->next;
        }
    } else if (current_policy == BEST_FIT) {
        while (curr != NULL) {
            if (curr->is_free && curr->size >= req_size) {
                if (chosen == NULL || curr->size < chosen->size) chosen = curr;
            }
            curr = curr->next;
        }
    } else if (current_policy == WORST_FIT) {
        while (curr != NULL) {
            if (curr->is_free && curr->size >= req_size) {
                if (chosen == NULL || curr->size > chosen->size) chosen = curr;
            }
            curr = curr->next;
        }
    } else if (current_policy == NEXT_FIT) {
        if (next_fit_rover == NULL) next_fit_rover = heap_head;
        Block *start = next_fit_rover;
        curr = start;
        do {
            if (curr->is_free && curr->size >= req_size) { chosen = curr; break; }
            curr = (curr->next != NULL) ? curr->next : heap_head;
        } while (curr != start);
    }

    if (chosen == NULL) return NULL;

    split_block(chosen, req_size);
    chosen->is_free = 0;
    next_fit_rover = (chosen->next != NULL) ? chosen->next : heap_head;

    return (void *)(chosen + 1);
}

int Mem_Free(void *ptr) {
    if (!is_initialized || ptr == NULL) return -1;

    Block *target = ((Block *)ptr) - 1;
    Block *curr = heap_head;
    int valid = 0;

    while (curr != NULL) {
        if (curr == target && !curr->is_free) { valid = 1; break; }
        curr = curr->next;
    }
    if (!valid) return -1;

    target->is_free = 1;
    coalesce_all();
    return 0;
}

int Mem_FreeByIndex(int index) {
    if (!is_initialized) return -1;
    Block *curr = heap_head;
    int i = 0;
    while (curr != NULL) {
        if (i == index) {
            if (curr->is_free) return -1;
            return Mem_Free((void *)(curr + 1));
        }
        i++;
        curr = curr->next;
    }
    return -1;
}

void Mem_Reset(void) {
    if (!is_initialized || heap_head == NULL) return;
    heap_head->size = mapped_total_size - sizeof(Block);
    heap_head->is_free = 1;
    heap_head->next = NULL;
    next_fit_rover = heap_head;
}

int Mem_ExportJSON(char *buf, size_t buf_size) {
    if (!is_initialized || buf == NULL || buf_size == 0) return -1;

    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset,
                       "{\"policy\":%d,\"header_size\":%zu,\"total_mmap\":%zu,\"blocks\":[",
                       current_policy, sizeof(Block), mapped_total_size);

    Block *curr = heap_head;
    int first = 1;
    while (curr != NULL) {
        char next_str[32];
        if (curr->next != NULL) {
            snprintf(next_str, sizeof(next_str), "\"%p\"", (void *)curr->next);
        } else {
            snprintf(next_str, sizeof(next_str), "\"NULL\"");
        }

        offset += snprintf(buf + offset, buf_size - offset,
                           "%s{\"addr\":\"%p\",\"payload_addr\":\"%p\",\"total_size\":%zu,\"data_size\":%zu,\"free\":%s,\"next\":%s,\"rover\":%s}",
                           first ? "" : ",",
                           (void *)curr,
                           (void *)(curr + 1),
                           curr->size + sizeof(Block),
                           curr->size,
                           curr->is_free ? "true" : "false",
                           next_str,
                           (curr == next_fit_rover) ? "true" : "false");
        first = 0;
        curr = curr->next;
    }

    offset += snprintf(buf + offset, buf_size - offset, "]}");
    return 0;
}

void Mem_Dump(void) {
    const char *names[] = {"FIRST_FIT", "BEST_FIT", "WORST_FIT", "NEXT_FIT"};
    size_t used_mem = 0, free_mem = 0;
    int used_blks = 0, free_blks = 0;

    Block *curr = heap_head;
    while (curr != NULL) {
        if (curr->is_free) {
            free_mem += curr->size + sizeof(Block);
            free_blks++;
        } else {
            used_mem += curr->size + sizeof(Block);
            used_blks++;
        }
        curr = curr->next;
    }

    printf("\n================================================================================\n");
    printf(" HEAP (1 MB MMAP) | Policy: %-9s | Used: %zu B (%d) | Free: %zu B (%d)\n",
           names[current_policy], used_mem, used_blks, free_mem, free_blks);
    printf("================================================================================\n");

    curr = heap_head;
    int idx = 1;
    while (curr != NULL) {
        const char *color = curr->is_free ? "\033[1;32m" : "\033[1;31m";
        const char *reset = "\033[0m";
        const char *rover = (curr == next_fit_rover) ? " \033[1;33m(rover)\033[0m" : "";
        char next_buf[24];
        if (curr->next) snprintf(next_buf, sizeof(next_buf), "%p", (void *)curr->next);
        else            snprintf(next_buf, sizeof(next_buf), "NULL");

        printf(" %p%s\n", (void *)curr, rover);
        printf(" %s+--------------------------+------------------+%s\n", color, reset);
        printf(" %s| HEADER (24B)             |                  |%s\n", color, reset);
        printf(" %s| size  %17zuB |       DATA       |%s\n", color, curr->size + sizeof(Block), reset);
        printf(" %s| free  %18s |   %11zuB   |%s\n", color, curr->is_free ? "true" : "false", curr->size, reset);
        printf(" %s| next  %18s |                  |%s\n", color, next_buf, reset);
        printf(" %s+--------------------------+------------------+%s\n", color, reset);
        printf(" block %d%s\n", idx++, curr->next ? "  -->" : "");

        curr = curr->next;
    }
    printf("================================================================================\n");
}