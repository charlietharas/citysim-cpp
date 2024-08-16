#pragma once

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <queue>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "drawable.h"
#include "macros.h"
#include "line.h"

class Train;

struct PathWrapper {
    Node* node;
    Line* line;
};

// std::pair is for losers
class NodePairWrapper {
    Node* first;
    Node* second;
public:
    NodePairWrapper(Node* f, Node* s) {
        first = f;
        second = s;
    }
    bool operator== (const NodePairWrapper& other) const {
        return first == other.first && second == other.second;
    }
    bool operator!= (const NodePairWrapper& other) const {
        return first != other.first || second != other.second;
    }
    struct HashFunction {
        std::size_t operator()(const NodePairWrapper& wrapper) const {
            return std::hash<Node*>()(wrapper.first) & std::hash<Node*>()(wrapper.second);
        }
    };
};

struct PathCacheWrapper {
    std::vector<PathWrapper> path;
public:
    PathCacheWrapper(PathWrapper* p, size_t s) {
        path.clear();
        for (int i = 0; i < s; i++) {
            path.push_back(p[i]);
        }
    }
    size_t pathSize() {
        return path.size();
    }
};

template<typename KeyType, typename ValueType>
class PathCache {
public:
    PathCache(size_t c) {
        capacity = c;
    }

    void put(const KeyType& key, const ValueType& value) {
        // check if key already exists
        auto it = cacheMap.find(key);
        if (it != cacheMap.end()) {
            // move to front (if exists)
            cacheList.erase(it->second);
            cacheMap.erase(it);
        }

        // move to front (if new)
        cacheList.push_front(std::make_pair(key, value));
        cacheMap[key] = cacheList.begin();

        if (cacheList.size() > capacity) {
            // remove LRU if cache above capacity
            auto last = cacheList.end();
            last--;
            cacheMap.erase(last->first);
            cacheList.pop_back();
        }
    }

    ValueType get(const KeyType& key) {
        auto it = cacheMap.find(key);
        if (it == cacheMap.end()) {
            return PathCacheWrapper(nullptr, 0);
        }

        // move to front (if accessed, exists)
        cacheList.splice(cacheList.begin(), cacheList, it->second);
        return it->second->second;
    }

    bool exists(const KeyType& key) const {
        return cacheMap.find(key) != cacheMap.end();
    }

    size_t capacity;
    std::list<std::pair<KeyType, ValueType>> cacheList;
    std::unordered_map<KeyType, typename std::list<std::pair<KeyType, ValueType>>::iterator, typename KeyType::HashFunction> cacheMap;
};


PathCache<NodePairWrapper, PathCacheWrapper> cache(PATH_CACHE_SIZE);

class Node : public Drawable {
public:
    Node() :
        Drawable(NODE_MIN_SIZE, NODE_N_POINTS) {
        for (int i = 0; i < N_NEIGHBORS; i++) {
            neighbors[i] = PathWrapper();
        }
        for (int i = 0; i < N_TRAINS; i++) {
            trains[i] = nullptr;
        }
    }

    unsigned int ridership;
    unsigned int capacity;
    double score;
    char status;
    char id[NODE_ID_SIZE];
    unsigned short int numerID;
    struct PathWrapper neighbors[N_NEIGHBORS];
    double weights[N_NEIGHBORS];
    Train* trains[N_TRAINS];

    bool addTrain(Train* train) {
        for (int i = 0; i < N_TRAINS; i++) {
            if (trains[i] == nullptr) {
                trains[i] = train;
                return true;
            }
        }
        return false;
    }

    bool removeTrain(Train* train) {
        for (int i = 0; i < N_TRAINS; i++) {
            if (trains[i] == train) {
                trains[i] = nullptr;
                return true;
            }
        }
        return false;
    }

    bool addNeighbor(PathWrapper* neighbor, float weight) {
        for (int i = 0; i < N_NEIGHBORS; i++) {
            if (neighbors[i].node == nullptr) {
                neighbors[i] = *neighbor;
                weights[i] = weight;
                return true;
            }
        }
        return false;
    }

    bool removeNeighbor(PathWrapper* neighbor) {
        for (int i = 0; i < N_NEIGHBORS; i++) {
            if (neighbors[i].node == neighbor->node && neighbors[i].line == neighbor->line) {
                neighbors[i] = PathWrapper({ nullptr, nullptr });
                return true;
            }
        }
        return false;
    }

    std::vector<PathWrapper> reconstructPath(std::unordered_map<Node*, PathWrapper*>& from, Node* end) {
        std::vector<PathWrapper> path;
        while (from.find(end) != from.end()) {
            PathWrapper pathWrapper = *from[end];
            path.push_back(pathWrapper);
            end = pathWrapper.node;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    PathCacheWrapper getCachedPath(Node* end) {
        // look up path in bidirectional cache
        PathCacheWrapper pcw = cache.get(NodePairWrapper(this, end));
        if (pcw.pathSize() > 0) {
            return pcw;
        }

        pcw = cache.get(NodePairWrapper(end, this));
        if (pcw.pathSize() > 0) {
            for (int i = 0; i < pcw.pathSize() / 2; i++) {
                PathWrapper temp = pcw.path[i];
                pcw.path[i] = pcw.path[pcw.pathSize() - i - 1];
                pcw.path[pcw.pathSize() - i - 1] = temp;
            }
            return pcw;
        }

        return PathCacheWrapper(nullptr, 0);
    }

    bool findPath(Node* end, PathWrapper* destPath, char* destPathSize) {
        PathCacheWrapper cachedPath = getCachedPath(end);
        if (cachedPath.pathSize() > 0) {
            std::copy(cachedPath.path.begin(), cachedPath.path.end(), destPath);
            *destPathSize = cachedPath.pathSize();
            if (cachedPath.pathSize() < CITIZEN_PATH_SIZE) {
                destPath[cachedPath.pathSize()] = PathWrapper();
            }
            return true;
        }

        auto compare = [](Node* a, Node* b) { return a->score > b->score; };
        std::priority_queue<Node*, std::vector<Node*>, decltype(compare)> queue(compare);
        std::unordered_set<Node*> queueSet;
        std::unordered_set<Node*> visited;
        std::unordered_map<Node*, PathWrapper*> from;
        std::unordered_map<Node*, double> score;

        score[this] = 0.0f;
        this->score = score[this] + this->dist(end);
        queue.push(this);
        queueSet.insert(this);

        while (!queue.empty()) {
            Node* current = queue.top();
            queue.pop();
            queueSet.erase(current);

            if (current == end) {
                std::vector<PathWrapper> path = reconstructPath(from, end);
                if (!path.empty()) {
                    path.push_back(PathWrapper{ end, path.back().line });
                }
                size_t pathSize = path.size();
                if (pathSize > CITIZEN_PATH_SIZE) {
                    std::cout << "Encountered large path (" << pathSize << ") [" << this->id << " : " << end->id << " ]" << std::endl;
                    return false;
                }

                std::copy(path.begin(), path.end(), destPath);
                *destPathSize = pathSize;
                if (pathSize < CITIZEN_PATH_SIZE) {
                    destPath[pathSize] = PathWrapper();
                }

                // TODO cache subpaths? would that even make sense?
                cache.put(NodePairWrapper(this, end), PathCacheWrapper(destPath, pathSize));

                return true;
            }

            visited.insert(current);

            for (int i = 0; i < N_NEIGHBORS; ++i) {
                Node* neighbor = current->neighbors[i].node;
                if (neighbor == nullptr) continue;
                Line* line = current->neighbors[i].line;

                if (visited.find(neighbor) != visited.end()) continue;

                double aggregateScore = score[current] + current->weights[i];

                // anti-transfer heuristic
                if (from.find(neighbor) != from.end() && from[neighbor]->line != line) {
                    aggregateScore += TRANSFER_PENALTY;
                }

                if (queueSet.find(neighbor) == queueSet.end() || aggregateScore < score[neighbor]) {
                    from[neighbor] = new PathWrapper{ current, line };
                    score[neighbor] = aggregateScore;
                    neighbor->score = score[neighbor] + neighbor->dist(end);

                    if (queueSet.find(neighbor) == queueSet.end()) {
                        queue.push(neighbor);
                        queueSet.insert(neighbor);
                    }
                }
            }
        }

        return false; // no path found
    }
};
