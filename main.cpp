#include <algorithm>
#include <chrono>
#include <iostream>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
using namespace std;

template <typename K, typename V>
class LRUCache {
private:
  // ---- Type aliases ----
  using Node = pair<K, V>;
  using ListIt = typename list<Node>::iterator;

  // ---- Data members ----
  int capacity;
  list<Node> items;
  unordered_map<K, ListIt> map;
  mutable mutex mtx;

public:
  // ---- Constructor ----
  LRUCache(int cap) : capacity(cap) {}

  // ---- Member functions ----
  optional<V> get(const K& key) {
    lock_guard<mutex> lock(mtx);
    auto it = map.find(key);
    if (it == map.end()) return nullopt;
    items.splice(items.begin(), items, it->second);
    return it->second->second;
  }

  void put(const K& key, const V& value) {
    lock_guard<mutex> lock(mtx);
    auto it = map.find(key);
    if (it != map.end()) {
      it->second->second = value;
      items.splice(items.begin(), items, it->second);
      return;
    }
    if ((int)items.size() == capacity) {
      map.erase(items.back().first);
      items.pop_back();
    }
    items.push_front({key, value});
    map[key] = items.begin();
  }

  int size() const {
    lock_guard<mutex> lock(mtx);
    return (int)items.size();
  }
};

template <typename K, typename V>
class ShardedLRUCache {
private:
  // ---- Data members ----
  vector<unique_ptr<LRUCache<K, V>>> shards;
  hash<K> hasher;

  LRUCache<K, V>& shardFor(const K& key) {
    return *shards[hasher(key) % shards.size()];
  }

public:
  // ---- Constructor ----
  ShardedLRUCache(int totalCapacity, int shardCount) {
    for (int i = 0; i < shardCount; i++)
      shards.push_back(make_unique<LRUCache<K, V>>(totalCapacity / shardCount));
  }

  // ---- Member functions ----
  optional<V> get(const K& key) { 
    return shardFor(key).get(key); 
  }

  void put(const K& key, const V& value){ 
    shardFor(key).put(key, value); 
  }

  int size() const {
    int total = 0;
    for (auto& s : shards) total += s->size();
    return total;
  }
};

// Runs totalOps operations split across 'threads' threads, returns milliseconds
double runBench(int threads, int totalOps, int shardCount) {
  ShardedLRUCache<int, int> cache(1000, shardCount);
  vector<thread> workers;
  int perThread = totalOps / threads;

  auto start = chrono::steady_clock::now();
  for (int t = 0; t < threads; t++) {
    workers.push_back(thread([&cache, t, perThread]() {
      for (int i = 0; i < perThread; i++) {
        int key = (i * 7 + t * 13) % 5000;     // 5000 keys, capacity 1000
        if (!cache.get(key)) cache.put(key, i);
      }
    }));
  }
  for (auto& w : workers) w.join();

    if (cache.size() > 1000)
    cout << "ERROR: cache exceeded capacity: " << cache.size() << "\n";

  auto end = chrono::steady_clock::now();

  return chrono::duration<double, milli>(end - start).count();
}

double median(vector<double> v) {
  sort(v.begin(), v.end());
  return v[v.size() / 2];
}

int main() {
  const int TOTAL_OPS = 400000, RUNS = 5, THREADS = 8;
  int shardCounts[] = {1, 4, 16, 64};

  runBench(THREADS, TOTAL_OPS, 1);  // warm-up

  cout << "Shards | Median ms (8 threads)" << endl;
  for (int s : shardCounts) {
    vector<double> times;
    for (int r = 0; r < RUNS; r++) times.push_back(runBench(THREADS, TOTAL_OPS, s));
    cout << "  " << s << "    | " << median(times) << endl;
  }
}