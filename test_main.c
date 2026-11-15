#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mymalloc.h"

void run_workload_benchmark(void) {
    const char *names[] = {"FIRST_FIT", "BEST_FIT ", "WORST_FIT", "NEXT_FIT "};
    const char *notes[] = {
        "Fastest start-of-heap scan ",
        "Minimum space waste        ",
        "Largest reusable splits    ",
        "Fast cyclic rover pointer  "
    };
    size_t reqs[] = {64, 128, 32, 256, 48, 512, 96, 16, 128, 64, 200, 80, 40, 160, 300, 72};
    int num_reqs = 16;

    printf("\n+---------------------------------------------------------------------------------------+\n");
    printf("| OBJECTIVE 2: WORKLOAD PERFORMANCE TUNING BENCHMARK (5,000 Iterations on 1 MB Heap)    |\n");
    printf("+------------+----------------+------------------+--------------------+-----------------+\n");
    printf("| Policy     | Time (ms)      | Throughput       | Success Rate       | Best For        |\n");
    printf("+------------+----------------+------------------+--------------------+-----------------+\n");

    for (int p = 0; p < 4; p++) {
        Mem_Reset();
        Mem_SetPolicy(p);
        void *ptrs[16] = {0};
        int success = 0, total = 0;

        clock_t start = clock();
        for (int iter = 0; iter < 5000; iter++) {
            for (int i = 0; i < num_reqs; i++) {
                ptrs[i] = Mem_Alloc(reqs[i]);
                total++;
                if (ptrs[i] != NULL) success++;
            }
            for (int i = 0; i < num_reqs; i += 2) {
                if (ptrs[i]) { Mem_Free(ptrs[i]); ptrs[i] = NULL; }
            }
            for (int i = 0; i < num_reqs; i += 2) {
                ptrs[i] = Mem_Alloc(reqs[num_reqs - 1 - i]);
                total++;
                if (ptrs[i] != NULL) success++;
            }
            for (int i = 0; i < num_reqs; i++) {
                if (ptrs[i]) { Mem_Free(ptrs[i]); ptrs[i] = NULL; }
            }
        }
        clock_t end = clock();
        double ms = ((double)(end - start) * 1000.0) / CLOCKS_PER_SEC;
        double ops_per_sec = (ms > 0) ? (total / (ms / 1000.0)) : 0;

        printf("| %s | %10.2f ms  | %10.0f ops/s | %6d / %-6d    | %.15s |\n",
               names[p], ms, ops_per_sec, success, total, notes[p]);
    }
    printf("+------------+----------------+------------------+--------------------+-----------------+\n");
}

void interactive_terminal(void) {
    char cmd[64];
    Mem_Dump();
    printf("\nInteractive Terminal Commands:\n");
    printf("  alloc <bytes>                  (e.g., alloc 512)\n");
    printf("  free <block_num>               (e.g., free 1)\n");
    printf("  policy <first|best|worst|next> (e.g., policy best)\n");
    printf("  holes                          (loads 3-hole demo)\n");
    printf("  bench                          (runs Objective 2 benchmark)\n");
    printf("  reset                          (resets 1MB heap)\n");
    printf("  quit                           (exit)\n");

    while (1) {
        printf("\nmymalloc> ");
        if (scanf("%63s", cmd) != 1) break;

        if (strcmp(cmd, "alloc") == 0) {
            size_t sz;
            if (scanf("%zu", &sz) == 1) {
                Mem_Alloc(sz);
                Mem_Dump();
            }
        } else if (strcmp(cmd, "free") == 0) {
            int bnum;
            if (scanf("%d", &bnum) == 1) {
                Mem_FreeByIndex(bnum - 1);
                Mem_Dump();
            }
        } else if (strcmp(cmd, "policy") == 0) {
            char pol[32];
            if (scanf("%31s", pol) == 1) {
                if (strcmp(pol, "first") == 0) Mem_SetPolicy(FIRST_FIT);
                else if (strcmp(pol, "best") == 0) Mem_SetPolicy(BEST_FIT);
                else if (strcmp(pol, "worst") == 0) Mem_SetPolicy(WORST_FIT);
                else if (strcmp(pol, "next") == 0) Mem_SetPolicy(NEXT_FIT);
                Mem_Dump();
            }
        } else if (strcmp(cmd, "holes") == 0) {
            Mem_Reset();
            Mem_SetPolicy(FIRST_FIT);
            size_t s[] = {256, 128, 512, 128, 128, 128};
            for (int i = 0; i < 6; i++) Mem_Alloc(s[i]);
            Mem_FreeByIndex(0);
            Mem_FreeByIndex(2);
            Mem_FreeByIndex(4);
            Mem_Dump();
        } else if (strcmp(cmd, "bench") == 0) {
            run_workload_benchmark();
        } else if (strcmp(cmd, "reset") == 0) {
            Mem_Reset();
            Mem_Dump();
        } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    if (Mem_Init(HEAP_SIZE_1MB) != 0) {
        printf("Mem_Init failed!\n");
        return 1;
    }

    if (argc > 1 && strcmp(argv[1], "-i") == 0) {
        interactive_terminal();
        return 0;
    }

    printf("=== Step 1: Initial 1 MB Heap (Single mmap call) ===\n");
    Mem_Dump();

    printf("\n=== Step 2: Creating 3 Free Holes (256B, 512B, 128B) ===\n");
    void *p1 = Mem_Alloc(256);
    void *p2 = Mem_Alloc(128);
    void *p3 = Mem_Alloc(512);
    void *p4 = Mem_Alloc(128);
    void *p5 = Mem_Alloc(128);
    void *p6 = Mem_Alloc(128);
    Mem_Free(p1);
    Mem_Free(p3);
    Mem_Free(p5);
    Mem_Dump();

    printf("\n=== Step 3: BEST_FIT (100B -> picks tightest 128B hole at block 5) ===\n");
    Mem_SetPolicy(BEST_FIT);
    void *b1 = Mem_Alloc(100);
    Mem_Dump();

    printf("\n=== Step 4: WORST_FIT (100B -> splits largest 1MB tail block) ===\n");
    Mem_SetPolicy(WORST_FIT);
    void *w1 = Mem_Alloc(100);
    Mem_Dump();

    printf("\n=== Step 5: FIRST_FIT (100B -> splits first 256B hole at block 1) ===\n");
    Mem_SetPolicy(FIRST_FIT);
    void *f1 = Mem_Alloc(100);
    Mem_Dump();

    printf("\n=== Step 6: NEXT_FIT (100B -> allocates at Rover) ===\n");
    Mem_SetPolicy(NEXT_FIT);
    void *n1 = Mem_Alloc(100);
    Mem_Dump();

    printf("\n=== Step 7: Free All & Coalesce Back to 1 MB ===\n");
    Mem_Free(p2); Mem_Free(p4); Mem_Free(p6);
    Mem_Free(b1); Mem_Free(w1); Mem_Free(f1); Mem_Free(n1);
    Mem_Dump();

    run_workload_benchmark();
    return 0;
}