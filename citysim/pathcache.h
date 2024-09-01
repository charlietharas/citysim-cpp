#pragma once

#include "node.h"

struct PathCacheWrapper {
    Node* startNode;
    Node* endNode;
    int size;
    int lru;
    PathWrapper path[CITIZEN_PATH_SIZE];
    PathCacheWrapper();
    PathCacheWrapper(Node* st, Node* e, PathWrapper* p, size_t s);
    void set(Node* st, Node* e, PathWrapper* p, size_t s, int l, bool reversePath);
    PathWrapper* begin();
    PathWrapper* end();
    int last();
};

class PathCache {
public:
    PathCache(size_t numBuckets, size_t bucketSize);
    ~PathCache();
    bool put(Node* start, Node* end, PathWrapper* p, size_t s, bool reversePath);
    PathCacheWrapper* get(Node* start, Node* end);
private:
    PathCacheWrapper* cache;
    size_t NUM_BUCKETS;
    size_t BUCKET_SIZE;
};
