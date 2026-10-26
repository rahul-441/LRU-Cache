# LRU-Cache

A C++17 LRU cache with O(1) `get` and `put`, written as a template, with a thread-safe version and a sharded version. Benchmarked to show the cost of lock contention.

## Design
- `list<pair<K,V>>` keeps usage order (front = most recent)
- `unordered_map<K, list::iterator>` gives O(1) lookup straight to a node
- `list::splice` moves a node to the front in O(1) with no copy, and iterators stay valid
- `get` returns `optional<V>`
- Thread-safe version: one `mutex` guards the list and map (even `get`, because it reorders the list)
- `ShardedLRUCache`: N independent caches, each with its own lock; key goes to shard `hash(key) % N`

## Usage
```cpp
LRUCache<string, string> cache(2);
cache.put("a", "apple");
cache.put("b", "banana");
cache.get("a");              // "a" is now most recent
cache.put("c", "cherry");    // evicts "b"
cout << cache.get("b").value_or("miss");   // miss
```

## Benchmark
400,000 operations (get, then put on a miss), 5,000 distinct keys, total capacity 1,000, median of 5 runs after a warm-up, `-O2`. Windows, MinGW GCC 8.1 (32-bit), 8 logical threads.

**Single global lock: same total work split across threads**

| Threads | Median | vs 1 thread |
|---|---|---|
| 1 | 359 ms | 1.0x |
| 2 | 1107 ms | 0.32x |
| 4 | 1973 ms | 0.18x |
| 8 | 2892 ms | 0.12x |

**Sharded cache at 8 threads** (separate run)

| Shards | Median | vs 1 shard |
|---|---|---|
| 1 | 1896 ms | 1.0x |
| 4 | 1207 ms | 1.6x faster |
| 16 | 768 ms | 2.5x faster |
| 64 | 691 ms | 2.7x faster |

## Findings
- One global lock made more threads slower: each operation is tiny, so threads spend their time contending for the lock.
- Sharding cut contention overhead by about 2.7x at 8 threads, with diminishing returns past 16 shards.
- Sharding did **not** beat the single-threaded run (359 ms) on this workload and platform, because lock and wake-up overhead still exceeds the work per operation.
- Contended mutex timings are noisy between runs (the 1-shard row differs from the 8-thread row above), so compare numbers only within one table.
- Trade-off: sharded eviction is approximate, since each shard evicts its own least recently used item.

## Build and run
```powershell
g++ -std=c++17 -O2 -pthread main.cpp -o output\main.exe
.\output\main.exe
```