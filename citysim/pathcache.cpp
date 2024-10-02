#include "pathcache.h"
#include <iostream>

PathCacheWrapper NULL_WRAPPER;

PathCacheWrapper::PathCacheWrapper() {
    startNode = nullptr;
    endNode = nullptr;
    memset(path, 0, sizeof(PathWrapper) * CITIZEN_PATH_SIZE);
    size = -1;
    lru = -1;
}

PathCacheWrapper::PathCacheWrapper(Node* st, Node* e, PathWrapper* p, int s) {
    set(st, e, p, s, -1);
}

void PathCacheWrapper::set(Node* st, Node* e, PathWrapper* p, int s, int l) {
    std::copy(p, p + s, path);

    startNode = st;
    endNode = e;
    size = s;
    lru = l;
}

PathWrapper* PathCacheWrapper::begin() {
    return &path[0];
}

PathWrapper* PathCacheWrapper::end() {
    return &path[last()];
}

int PathCacheWrapper::last() {
    return size - 1;
}

PathCache::PathCache(size_t numBuckets, size_t bucketSize) {
    cache = new PathCacheWrapper[numBuckets * bucketSize];
    NUM_BUCKETS = numBuckets;
    BUCKET_SIZE = bucketSize;
}

PathCache::~PathCache() {
    delete(cache);
}

// returns true if a cache entry was evicted
bool PathCache::put(Node* start, Node* end, PathWrapper* p, int s) {
    int bucket = (start->numerID * PRIME_1 + end->numerID * PRIME_2) % NUM_BUCKETS;
    int bucketInd = bucket * BUCKET_SIZE;
    int maxLRU = -1;
    int maxInd = 0;
    for (int i = 0; i < BUCKET_SIZE; i++) {
        int ind = bucketInd + i;
        cache[ind].lru++;
        int lru = cache[ind].lru;
        if (cache[ind].startNode == start && cache[ind].endNode == end) {
            return false;
        }
        if (lru == -1) {
            cache[ind].set(start, end, p, s, 0);
            return false;
        }
        if (lru > maxLRU) {
            maxLRU = lru;
            maxInd = ind;
        }
    }
    cache[maxInd].set(start, end, p, s, 0);
    return true;
}

PathCacheWrapper& PathCache::get(Node* start, Node* end) {
    int bucket = (start->numerID * PRIME_1 + end->numerID * PRIME_2) % NUM_BUCKETS;
    int bucketInd = bucket * BUCKET_SIZE;
    for (int i = 0; i < BUCKET_SIZE; i++) {
        int ind = bucketInd + i;
        cache[ind].lru++;
        if (cache[ind].startNode == start && cache[ind].endNode == end) {
            cache[ind].lru = 0;
            for (int j = i + 1; j < BUCKET_SIZE; j++) {
                cache[bucketInd + j].lru++;
            }
            return cache[ind];
        }
    }

    NULL_WRAPPER.size = 0; // because somehow this mf is getting updated... that's probably an issue
    return NULL_WRAPPER;
}

