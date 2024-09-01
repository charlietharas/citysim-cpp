#include "pathcache.h"

PathCacheWrapper NULL_WRAPPER;

PathCacheWrapper::PathCacheWrapper() {
    set(nullptr, nullptr, nullptr, 0, -1, false);
}

PathCacheWrapper::PathCacheWrapper(Node* st, Node* e, PathWrapper* p, int s) {
    set(st, e, p, s, -1, false);
}

void PathCacheWrapper::set(Node* st, Node* e, PathWrapper* p, int s, int l, bool reversePath) {
    if (reversePath) {
        for (size_t i = 0; i < s-1; i++) {
            path[i].node = p[s-i-1].node;
            path[i].line = p[s - i - 2].line;
        }
        path[s - 1] = p[0];
    }
    else {
        for (size_t i = 0; i < s; i++) {
            path[i] = p[i];
        }
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

bool PathCache::put(Node* start, Node* end, PathWrapper* p, int s, bool reversePath) {
    int bucket = (start->numerID + end->numerID) % NUM_BUCKETS;
    int bucketInd = bucket * BUCKET_SIZE;
    int max = -1;
    int maxInd = 0;
    for (int i = 0; i < BUCKET_SIZE; i++) {
        int ind = bucketInd + i;
        int lru = cache[ind].lru;
        if (lru == -1) {
            cache[ind].set(start, end, p, s, 0, reversePath);
            return false;
        }
        if (lru > max) {
            max = lru;
            maxInd = ind;
        }
    }
    cache[maxInd].set(start, end, p, s, 0, reversePath);
    return true;
}

PathCacheWrapper& PathCache::get(Node* start, Node* end) {
    int bucket = (start->numerID + end->numerID) % NUM_BUCKETS;
    int bucketInd = bucket * BUCKET_SIZE;
    for (int i = 0; i < BUCKET_SIZE; i++) {
        int ind = bucketInd + i;
        if (cache[bucketInd].startNode == start && cache[bucketInd].endNode == end) {
            cache[bucketInd].lru = 0;
            return cache[bucketInd];
        }
    }

    return NULL_WRAPPER;
}

