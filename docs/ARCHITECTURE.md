# Architecture & Design Guide

## System Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│            Web Browser / Client                         │
├─────────────────────────────────────────────────────────┤
│                   HTTP / REST                           │
└──────────────────────────┬──────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│              REST API Server (main.c)                   │
│  - HTTP Request Handler                                │
│  - Multi-threaded Client Handler                       │
│  - Route Dispatcher                                    │
├─────────────────────────────────────────────────────────┤
│  Endpoints:                                            │
│  ✓ GET  /api/status                                    │
│  ✓ POST /api/malloc                                    │
│  ✓ POST /api/free                                      │
│  ✓ POST /api/reset                                     │
│  ✓ POST /api/verify                                    │
└──────────────────────────┬──────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│         Allocator Instance (Mutex Protected)           │
├─────────────────────────────────────────────────────────┤
│  pthread_mutex_t lock                                  │
│  char *heap [4096 bytes]                               │
│  Block *head                                           │
│  AllocatorStats stats                                  │
└──────────────────────────┬──────────────────────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
         ▼                 ▼                 ▼
   ┌──────────┐      ┌──────────┐     ┌──────────┐
   │ Best-Fit │  OR  │First-Fit │     │  Utils   │
   │ (O(n))   │      │(O(k))    │     │Functions │
   └──────────┘      └──────────┘     └──────────┘

        Heap Memory (4KB)
     ┌──────────────────────┐
     │ Block₁  │ Block₂ │...│
     └──────────────────────┘
```

## Component Breakdown

### 1. Allocator Core
**Files**: `allocator.h`, `best_fit.c`, `first_fit.c`

**Responsibilities**:
- Memory block allocation
- Memory deallocation with coalescing
- Heap verification
- Statistics tracking

**Key Structure**:
```c
typedef struct {
    char *heap;                // 4KB memory pool
    Block *head;               // Linked list head
    pthread_mutex_t lock;      // Thread synchronization
    AllocatorStats stats;      // Performance metrics
} Allocator;
```

### 2. Utility Functions
**File**: `utils.c`

**Functions**:
- `update_allocator_stats()` - Recalculate metrics
- `print_heap_visualization()` - ASCII visualization
- `export_heap_state_json()` - JSON export
- `analyze_memory_pattern()` - Pattern analysis

### 3. REST API Server
**File**: `main.c`

**Architecture**:
```
Main Thread (Server Loop)
    │
    ├─ Create Server Socket
    ├─ Listen on Port 8080
    │
    └─ Accept Client Connection
        │
        ├─ Create Client Thread
        │  └─ Parse HTTP Request
        │  └─ Dispatch to Handler
        │  └─ Send JSON Response
        │  └─ Close Connection
        │
        └─ Detach Thread
```

**Threading Model**:
- Main thread: Accept connections
- Per-client threads: Handle requests
- Lock-free design (handlers use allocator's mutex)

### 4. Web GUI
**Files**: `web/index.html`, `web/style.css`, `web/app.js`

**Responsibilities**:
- Real-time heap visualization
- User interaction management
- API communication
- Statistics display

## Memory Layout

### Heap Structure
```
Memory Address         Content
┌──────────────────────────────┐
│ 0x7fff0000 (Allocator Base)  │
├──────────────────────────────┤
│ [Block₁ Header]              │  ←─── Block {
│  Size: 256                   │      size: 256
│  is_free: false              │      is_free: false
│  next: →Block₂               │      next: →
│ [Block₁ Data (256 bytes)]    │     }
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
├──────────────────────────────┤
│ [Block₂ Header]              │  ←─── Block {
│  Size: 512                   │      size: 512
│  is_free: false              │      is_free: false
│  next: →Block₃               │      next: →
│ [Block₂ Data (512 bytes)]    │     }
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
├──────────────────────────────┤
│ [Block₃ Header]              │  ←─── Block {
│  Size: 3072                  │      size: 3072
│  is_free: true               │      is_free: true
│  next: NULL                  │      next: NULL
│ [Block₃ Data (3072 bytes)]   │     }
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
├──────────────────────────────┤
│ 0x7fff1000 (Heap End)        │
└──────────────────────────────┘

Total: 4096 bytes
Metadata Per Block: 32 bytes
```

## Allocation Algorithms

### Best-Fit Strategy
```
Algorithm: find_best_fit_block(head, size)
───────────────────────────────────────
Input: Linked list head, requested size
Output: Block with minimum waste

Time Complexity: O(n) where n = number of blocks
Space Complexity: O(1)

Steps:
1. Initialize: best_block = NULL, min_waste = ∞
2. Iterate through linked list:
   FOR each block in list:
      IF (block.is_free AND block.size >= size):
         waste = block.size - size
         IF (waste < min_waste):
            min_waste = waste
            best_block = block
3. Return best_block

Characteristics:
✓ Low fragmentation
✓ Maximizes large block preservation
✗ Higher latency (full scan required)
✗ Predictability (O(n) worst case)

Use Case: General-purpose systems where memory
          utilization matters more than latency
```

### First-Fit Strategy
```
Algorithm: find_first_fit_block(head, size)
────────────────────────────────────────────
Input: Linked list head, requested size
Output: First block that fits

Time Complexity: O(k) where k = blocks until fit
Space Complexity: O(1)

Steps:
1. Iterate through linked list:
   FOR each block in list:
      IF (block.is_free AND block.size >= size):
         RETURN block  ← Early exit!
2. RETURN NULL (not found)

Characteristics:
✓ Lower latency (early exit possible)
✓ Predictable performance
✗ Higher fragmentation
✗ Wastes large blocks

Use Case: Real-time systems where allocation
          latency must be deterministic
```

## Thread Safety

### Mutex-Protected Critical Sections
```c
// Example: Thread-safe malloc
void *allocator_malloc(Allocator *alloc, size_t size) {
    pthread_mutex_lock(&alloc->lock);
    
    try {
        // Critical section: heap state modifications
        Block *block = find_free_block(alloc->head, size);
        
        if (block) {
            block->is_free = false;
            alloc->stats.num_allocations++;
            alloc->stats.total_allocated += size;
        }
    } finally {
        pthread_mutex_unlock(&alloc->lock);
    }
    
    return ptr;
}
```

### Lock Contention
```
Thread 1         Thread 2         Thread 3
   │                │                │
   ├─ Lock          │                │
   ├─ Malloc        │                │
   ├─ Unlock        ├─ Wait for lock │
   │                ├─ Lock          ├─ Wait for lock
   │                ├─ Malloc        │
   │                ├─ Unlock        ├─ Lock
   │                │                ├─ Malloc
   │                │                ├─ Unlock
```

**Optimization Strategies**:
1. Keep critical section small
2. Batch multiple operations
3. Use lock-free algorithms (future)
4. Segregated free lists (future)

## Fragmentation Management

### Coalescing Strategy
```
Before Coalescing:
┌─────────┐ ┌──────┐ ┌──────┐ ┌─────────┐
│Allocated│ │ Free │ │ Free │ │Allocated│
│  256B   │ │ 256B │ │ 256B │ │  512B   │
└─────────┘ └──────┘ └──────┘ └─────────┘

After Coalescing:
┌─────────┐ ┌─────────────┐ ┌─────────┐
│Allocated│ │    Free     │ │Allocated│
│  256B   │ │    512B     │ │  512B   │
└─────────┘ └─────────────┘ └─────────┘

Algorithm:
1. Scan linked list
2. For adjacent free blocks in memory:
   - Merge into single block
   - Update size
   - Update next pointer
3. Continue until no more merges

Complexity: O(n) per free operation
```

### Fragmentation Metrics
```
Fragmentation Ratio = (Free Blocks / Total Blocks) × 100%

Example:
- 5 Total Blocks
- 3 Free Blocks
- Fragmentation = 60%

High Fragmentation (>50%):
- Many small free blocks
- Allocation success lower
- Coalescing beneficial

Low Fragmentation (<30%):
- Few free blocks
- Allocations more likely to succeed
- System healthy
```

## Request Flow

### Malloc Request Flow
```
Client Request
    │
    ▼
HTTP Handler
    │
    ├─ Parse JSON: {"size": 256}
    │
    ▼
allocator_malloc(alloc, 256)
    │
    ├─ Lock mutex
    │
    ├─ Find free block (O(n) or O(k))
    │     ↓
    │  Block found? ─NO→ Update failed count
    │     │            │
    │     YES          ▼
    │     │        Return NULL
    │     │
    │     ▼
    │  Split if excess space
    │     │
    │     ▼
    │  Mark as allocated
    │     │
    │     ▼
    │  Update statistics
    │     │
    │     ▼
    │  Unlock mutex
    │
    ▼
Return pointer to server
    │
    ▼
Format JSON response
    │
    ▼
Send to client
```

### Free Request Flow
```
Client Request
    │
    ▼
HTTP Handler
    │
    ├─ Parse JSON: {"address": "0x..."}
    │
    ▼
allocator_free(alloc, ptr)
    │
    ├─ Lock mutex
    │
    ├─ Get block header
    │     ↓
    │  Valid? ─NO→ Return error
    │     │
    │     YES
    │     │
    │     ▼
    │  Mark as free
    │     │
    │     ▼
    │  Update statistics
    │     │
    │     ▼
    │  Coalesce adjacent blocks (O(n))
    │     │
    │     ▼
    │  Unlock mutex
    │
    ▼
Return success to server
    │
    ▼
Send JSON response
    │
    ▼
Client UI updates
```

## Performance Characteristics

### Time Complexity
| Operation | Best-Fit | First-Fit | Coalesce |
|-----------|----------|-----------|----------|
| Malloc    | O(n)     | O(k)      | —        |
| Free      | —        | —         | O(n)     |
| Verify    | O(n)     | O(n)      | —        |
| Reset     | O(1)     | O(1)      | —        |

### Space Complexity
| Structure | Space |
|-----------|-------|
| Heap      | 4KB fixed |
| Metadata/block | 32 bytes |
| Overhead | ~3% (32 bytes × blocks) |

### Throughput
```
Sequential Allocations:
├─ Malloc:  ~1-10ms  (depends on strategy)
├─ Free:    ~2-15ms  (includes coalescing)
├─ Reset:   <1ms     (O(1) operation)
└─ Verify:  ~1-5ms   (O(n) scan)

Concurrent (4 threads):
├─ Throughput: ~100-200 req/sec
├─ Lock contention: Low to medium
├─ Tail latency: Higher due to blocking
└─ Memory overhead: Minimal
```

## Deployment Architecture

### Local Deployment
```
OS
 └─ ./build/bin/allocator_server
     └─ Port 8080 (REST API)
        └─ http://localhost:8080 (Web GUI)
```

### Docker Deployment
```
Docker Host
 └─ Container: custom-allocator:latest
     └─ Process: /app/allocator_server
         └─ Port 8080:8080 (mapped)
            └─ http://localhost:8080 (Web GUI)
```

### Docker Compose Deployment
```
Docker Compose
 ├─ memory-allocator (REST API + Web GUI)
 ├─ prometheus (metrics collection)
 └─ grafana (visualization)
```

## Extension Points

### Future Enhancements
1. **Segregated Free Lists**
   - Multiple lists indexed by size classes
   - O(1) allocation instead of O(n)
   - Trade-off: more memory overhead

2. **Boundary Tags**
   - Footer metadata in each block
   - O(1) coalescing
   - Trade-off: doubled metadata size

3. **Thread-Local Allocators**
   - Per-thread memory pools
   - Reduced lock contention
   - Trade-off: memory fragmentation

4. **Mmap Support**
   - Dynamic heap resizing
   - Handle allocations > 4KB
   - Trade-off: OS page alignment

5. **Memory Compression**
   - Compact allocations
   - Reduce fragmentation
   - Trade-off: CPU overhead

---

## References
- [README.md](../README.md) - Project overview
- [API.md](./API.md) - REST API documentation
- [BENCHMARKS.md](./BENCHMARKS.md) - Performance data
