#pragma once

#include "node.h"
#include <cstdint>

struct PathCacheWrapper {
    uint16_t startNode;
    uint16_t endNode;
    int size;
    int lru;
    PathWrapper path[CITIZEN_PATH_SIZE];

    PathCacheWrapper();
    PathCacheWrapper(uint16_t st, uint16_t e, PathWrapper* p, int s);

    void set(uint16_t st, uint16_t e, PathWrapper* p, int s, int l);

    PathWrapper* begin();
    PathWrapper* end();
    int last();
};

class PathCache {
public:
    PathCache(size_t numBuckets, size_t bucketSize);
    ~PathCache();

    bool put(uint16_t start, uint16_t end, PathWrapper* p, int s);
    PathCacheWrapper& get(uint16_t start, uint16_t end);
private:
    PathCacheWrapper* cache;
    size_t NUM_BUCKETS;
    size_t BUCKET_SIZE;
};