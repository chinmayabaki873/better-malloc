# A “Better” Malloc — *Inside the Heap*

**Author:** Chinmaya Venkata Sai Baki  
**Affiliation:** Department of Computer Science & Engineering, SRM University AP  
**Repository:** [github.com/chinmayabaki873/better-malloc](https://github.com/chinmayabaki873/better-malloc)  
**Live Interactive Studio:** [https://better-malloc-chinmaya.onrender.com](https://better-malloc-chinmaya.onrender.com)

---

## Overview

This project implements a custom user-level heap memory allocator (`libmymalloc.so`) in C for a Linux process, accompanied by both an ASCII terminal visualizer (`Mem_Dump`) and a live web-based virtual memory inspector connected directly to the compiled C shared library via `ctypes`.

Operating entirely within the virtual address space of a single user process, the allocator performs the two core responsibilities of a runtime heap manager:

1. **OS Address Space Acquisition:** Requests a contiguous region of anonymous virtual memory from the Linux kernel using `mmap()` strictly once during initialization (`Mem_Init`).
2. **User-Space Free List Bookkeeping:** Tracks allocated and free chunks using in-band 24-byte metadata headers (`struct Block`), dynamically splitting blocks upon allocation (`Mem_Alloc`), validating and reclaiming blocks upon deallocation (`Mem_Free`), and immediately coalescing adjacent free blocks to prevent external fragmentation.

---

## How This Project Fulfills All Three Core Objectives

### Objective 1: Understanding the Nuances of Building a Memory Allocator

Building a user-space allocator requires addressing low-level architectural constraints and bookkeeping hazards:

* **Single `mmap()` Initialization & Page Alignment:** `Mem_Init(size_t size_of_region)` rounds the requested region up to the nearest multiple of the OS virtual page size (`sysconf(_SC_PAGESIZE)`, typically 4,096 bytes) and invokes `mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)` only once. Subsequent calls to `Mem_Init` are rejected (`is_initialized` guard), ensuring the allocator operates within its fixed 1 MB (`1,048,576` bytes) arena.

* **In-Band Metadata Header (24 Bytes):** Each chunk of memory is prefixed with a 24-byte `Block` structure stored directly inside the mapped heap region immediately preceding the user payload pointer:

  ```c
  typedef struct Block {
      size_t size;          // 8 bytes: Usable payload size (excluding header)
      int is_free;          // 4 bytes + 4 bytes struct alignment padding
      struct Block *next;   // 8 bytes: Pointer to the next contiguous block
  } Block;                  // Total = 24 bytes on a typical 64-bit system
  ```

* **8-Byte Word Alignment:** Every requested size is rounded up to the nearest multiple of 8 bytes using bitwise alignment:

  ```c
  static size_t align8(size_t size) {
      return (size + 7) & ~((size_t)7);
  }
  ```

* **Dynamic Block Splitting (`split_block`):** When a selected free block exceeds the aligned request by at least `sizeof(Block) + 8` (32 bytes), the allocator carves a new free block out of the excess tail bytes rather than wasting the entire hole to internal fragmentation.

* **Strict Pointer Validation & Immediate Coalescing (`coalesce_all`):** `Mem_Free(void *ptr)` computes the block header address `((Block *)ptr) - 1`, verifies that the block exists in the heap list and is currently allocated (preventing double-free and invalid-pointer corruption), marks it free, and immediately merges any contiguous free neighbors (`curr` and `curr->next`) back into a single unified block.

### Objective 2: The Art of Performance Tuning for Different Workloads

No single placement policy is optimal for every allocation pattern. Our allocator implements four runtime-switchable placement strategies via `Mem_SetPolicy(int policy)` and evaluates them under a 5,000-iteration fragmented stress workload:

| Placement Policy | Search Mechanism | Time Complexity | Workload Tuning Tradeoff |
|---|---|---|---|
| First Fit (`FIRST_FIT`) | Scans from `heap_head` and selects the first free block with size >= request size. | O(N), fast early exit | Fast average allocation speed, but tends to accumulate small splinter fragments near the beginning of the heap. |
| Best Fit (`BEST_FIT`) | Exhaustively scans the list to select the free block that minimizes leftover space. | O(N), full scan | Preserves large contiguous blocks for future large requests, but exhaustive scanning increases search time and can create tiny unusable remainders. |
| Worst Fit (`WORST_FIT`) | Exhaustively scans the list to select the largest available free block. | O(N), full scan | Leaves behind large split remainders that are more likely to satisfy subsequent medium-sized requests, at the cost of breaking up the largest contiguous block early. |
| Next Fit (`NEXT_FIT`) | Maintains a roving pointer (`next_fit_rover`) and resumes searching from the block where the last allocation finished. | O(N) worst case | Avoids repeatedly traversing allocated blocks at the beginning of the heap, potentially improving throughput in cyclic workloads. |

### Objective 3: Creating and Linking a Shared Library (`libmymalloc.so`)

Rather than compiling the allocator statically into a single program, the project builds a standard POSIX Position-Independent Code (`-fPIC`) shared object library (`libmymalloc.so`) that can be linked by C executables or loaded dynamically by high-level language runtimes.

**Compilation as a Position-Independent Shared Object:**

```bash
gcc -Wall -Wextra -O2 -fPIC -shared -o libmymalloc.so mymalloc.c
```

**Dynamic Linking with Runtime Library Path (rpath):**

```bash
gcc -Wall -Wextra -O2 -o test_main test_main.c -L. -lmymalloc -Wl,-rpath,.
```

**Foreign Function Interface (FFI) Integration:**

`server.py` loads `./libmymalloc.so` directly into Python using `ctypes.CDLL("./libmymalloc.so")`, exposing live C virtual memory addresses to the web studio in real time.

---

## Repository Structure

```text
better-malloc/
├── mymalloc.h        # Public C API declarations, fit policy macros, and Block struct
├── mymalloc.c        # Core allocator implementation
├── test_main.c       # Automated tests, interactive CLI, and workload benchmark
├── Makefile          # Build system for libmymalloc.so and test_main
├── server.py         # Python ctypes HTTP bridge to libmymalloc.so
├── visualizer.html   # Interactive 3-tab web studio
├── Dockerfile        # Ubuntu 22.04 container specification for Render
└── README.md         # Project documentation
```

---

## How to Build & Run Locally (Linux / WSL)

### 1. Compile the Shared Library and Test Suite

```bash
make
```

### 2. Run the Automated Terminal Visualizer & Workload Benchmark

```bash
./test_main
```

Executes the fragmentation demonstration across all four policies, prints color-coded ASCII `HEADER (24B) | DATA` block diagrams in the terminal, and outputs the 5,000-iteration benchmark table.

### 3. Run Interactive Terminal Mode (CLI)

```bash
./test_main -i
```

Available CLI commands:

```text
alloc <bytes>
free <block_num>
policy <first|best|worst|next>
holes
bench
reset
quit
```

### 4. Launch the Live Web Visualizer Connected to `libmymalloc.so`

```bash
python3 server.py
```

Open the following address in your browser:

https://better-malloc.onrender.com/

Every click in the browser calls the compiled C functions in `libmymalloc.so` and prints the corresponding `Mem_Dump()` output in your terminal simultaneously.

---

**Designed & Built by Chinmaya Venkata Sai Baki • SRM University AP**
