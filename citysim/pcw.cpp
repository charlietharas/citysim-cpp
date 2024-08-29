#include "pcw.h"

PathCacheWrapper::PathCacheWrapper() {
    set(nullptr, nullptr, nullptr, 0, -1);
}

PathCacheWrapper::PathCacheWrapper(Node* st, Node* e, PathWrapper* p, size_t s) {
    set(st, e, p, s, -1);
}

void PathCacheWrapper::set(Node* st, Node* e, PathWrapper* p, size_t s, int l) {
    for (size_t i = 0; i < s; i++) {
        path[i] = p[i];
    }
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

size_t PathCacheWrapper::last() {
    return size - 1;
}

PathCache::PathCache(size_t numBuckets, size_t bucketSize) {
    cache = new PathCacheWrapper[numBuckets * bucketSize];
    for (int i = 0; i < numBuckets * bucketSize; i++) {
        cache[i] = PathCacheWrapper();
    }
    NUM_BUCKETS = numBuckets;
    BUCKET_SIZE = bucketSize;
}

PathCache::~PathCache() {
    delete(cache);
}

bool PathCache::put(Node* start, Node* end, PathWrapper* p, size_t s) {
    int bucket = (start->numerID + end->numerID) % NUM_BUCKETS;
    int bucketInd = bucket * BUCKET_SIZE;
    int max = -1;
    int maxInd = 0;
    for (int i = 0; i < BUCKET_SIZE; i++) {
        int ind = bucketInd + i;
        int lru = cache[ind].lru;
        if (lru == -1) {
            cache[ind].set(start, end, p, s, 0);
            return false;
        }
        if (lru > max) {
            max = lru;
            maxInd = ind;
        }
    }
    cache[maxInd].set(start, end, p, s, 0);
    return true;
}

PathCacheWrapper* PathCache::get(Node* start, Node* end) {
    int bucket = (start->numerID + end->numerID) % NUM_BUCKETS;
    int bucketInd = bucket * BUCKET_SIZE;
    for (int i = 0; i < BUCKET_SIZE; i++) {
        int ind = bucketInd + i;
        if (cache[bucketInd].startNode == start && cache[bucketInd].endNode == end) {
            cache[bucketInd].lru = 0;
            return &cache[bucketInd];
        }
    }
    return nullptr;
}

