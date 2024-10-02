#include "pathcache.h"
#include <iostream>

PathCacheWrapper NULL_WRAPPER;

PathCacheWrapper::PathCacheWrapper() {
    set(nullptr, nullptr, nullptr, 0, -1);
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

bool PathCache::put(Node* start, Node* end, PathWrapper* p, int s) {
    int bucket = (start->numerID + end->numerID) % NUM_BUCKETS;
    int bucketInd = bucket * BUCKET_SIZE;
    int max = -1;
    int maxInd = 0;
    for (int i = 0; i < BUCKET_SIZE; i++) {
        int ind = bucketInd + i;
        int lru = cache[ind].lru;
        if (cache[ind].startNode == start && cache[ind].endNode == end) {
            return false;
        }
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

PathCacheWrapper& PathCache::get(Node* start, Node* end) {
    int bucket = (start->numerID + end->numerID) % NUM_BUCKETS;
    int bucketInd = bucket * BUCKET_SIZE;
    for (int i = 0; i < BUCKET_SIZE; i++) {
        int ind = bucketInd + i;
        if (cache[ind].startNode == start && cache[ind].endNode == end) {
            cache[ind].lru = 0;
            return cache[ind];
        }
    }

    return NULL_WRAPPER;
}

