# NanoMatch: Sub-50ns Low-Latency Order Matching Engine

**NanoMatch** is a highly optimized, multi-threaded C++17 limit order book (LOB) and matching engine designed for deterministic, ultra-low latency execution. Built with strict mechanical sympathy, the engine achieves an end-to-end pipeline latency of **~40 nanoseconds per order** under high-throughput conditions.

This project bypasses the operating system kernel entirely in its critical path, eliminating heap allocations, mutex locks, and O(n) data structure scans to guarantee absolute O(1) worst-case matching times.

## 🚀 Performance Benchmarks
Tested on [Insert Your CPU Model, e.g., Intel Core i7 / AMD Ryzen 7] processing 500,000 simulated orders with an ~80% fill rate:
* **Pipeline Latency:** ~40.6 ns / order
* **Total Execution Time:** ~20.3 ms (500K orders)
* **Concurrency Model:** Lock-free, kernel-bypass SPSC architecture
* **Algorithmic Complexity:** O(1) insertion, cancellation, and execution

---

## 🧠 Core Architecture: The Four Pillars

To achieve sub-microsecond latency, NanoMatch relies on four core architectural pillars designed to maximize CPU cache warmth and prevent OS context switching.

### 1. Zero-Lock Concurrency (SPSC Ring Buffer)
Standard threading models rely on `std::mutex`, which introduces severe latency spikes due to kernel-level context switches. NanoMatch isolates the network/ingestion thread from the matching core using a Single-Producer Single-Consumer (SPSC) ring buffer.
* **Memory Ordering:** Utilizes strict `std::memory_order_acquire` and `std::memory_order_release` semantics for zero-cost thread synchronization.
* **False Sharing Prevention:** The read and write atomic indices are explicitly aligned to 64-byte boundaries (`alignas(64)`) to ensure they reside on separate CPU cache lines, preventing cache invalidation ping-pong between cores.

### 2. Zero-Allocation Memory (Object Pools)
Dynamic memory allocation (`new`/`delete`) during trading hours fragments the heap and forces kernel mode transitions. 
* NanoMatch allocates a massive contiguous block of memory at startup capable of holding millions of `Order` structs. 
* Order allocation and deallocation during runtime is managed via a pre-computed array of free indices, effectively reducing memory management to a single O(1) pointer assignment.

### 3. O(1) Cancellations (Intrusive Linked Lists)
Traditional order books often use `std::map` or `std::vector`, which destroy CPU cache locality and require O(log n) or O(n) scans to find and cancel deep book orders.
* NanoMatch uses flat arrays indexed by price ticks for O(1) price level lookups.
* Orders at a specific price level are chained using an **Intrusive Doubly Linked List**. The `Order` struct itself contains the `next` and `prev` pointers. When an order is canceled, it can unlink itself from the book in absolute O(1) time without any traversal.

### 4. Hardware-Accelerated Price Tracking (Hierarchical Bitsets)
When a price level is depleted, finding the next best bid or ask using a `while` loop creates unpredictable O(n) latency spikes, especially during wide market spreads.
* NanoMatch maps all 4,096 supported price levels to a 64x64 bitmask grid.
* By leveraging compiler intrinsics (`__builtin_clzll` and `__builtin_ctzll`), the engine maps the search for the next active price level directly to single-cycle CPU hardware instructions (like `LZCNT` or `TZCNT` on x86). This guarantees an O(1) search time regardless of how wide the spread is.

---

## 🛠️ Building and Running

Because NanoMatch relies on hardware-specific intrinsics and strict cache alignment, it must be compiled with specific optimization flags.

**Prerequisites:**
* GCC or Clang compiler with C++17 support
* Linux or macOS (Windows MSVC requires minor intrinsic mapping)

**Compilation:**
```bash
# -O3 for maximum optimization
# -march=native to enable specific CPU hardware instructions
# -pthread to link the threading library
g++ -O3 -march=native -std=c++17 -pthread hft_engine_threaded.cpp -o hft_engine
