#include <iostream>
#include "node.h"
#include "pathcache.h"

PathCache cache = PathCache(PATH_CACHE_BUCKETS, PATH_CACHE_BUCKETS_SIZE);

int pathRequests;
int pathCacheHits;
int pathFails;

Node::Node() : Drawable(NODE_MIN_SIZE, NODE_N_POINTS) {
    numNeighbors = 0;
    for (int i = 0; i < NODE_N_NEIGHBORS; i++) {
        neighbors[i] = PathWrapper();
    }
    for (int i = 0; i < NODE_N_TRAINS; i++) {
        trains[i] = nullptr;
    }
}

bool Node::addTrain(Train* train) {
    for (int i = 0; i < NODE_N_TRAINS; i++) {
        if (trains[i] == nullptr) {
            trains[i] = train;
            return true;
        }
    }
    return false;
}

bool Node::removeTrain(Train* train) {
    for (int i = 0; i < NODE_N_TRAINS; i++) {
        if (trains[i] == train) {
            trains[i] = nullptr;
            return true;
        }
    }
    return false;
}

bool Node::addNeighbor(const PathWrapper& neighbor, float weight) {
    for (int i = 0; i < NODE_N_NEIGHBORS; i++) {
        if (neighbors[i].node == neighbor.node && neighbors[i].line == neighbor.line) {
            return false;
        }
        if (neighbors[i].node == nullptr) {
            neighbors[i] = neighbor;
            weights[i] = weight;
            numNeighbors++;
            return true;
        }
    }
    std::cout << "Too big @" << id << std::endl;
    return false;
}

bool Node::removeNeighbor(const PathWrapper& neighbor) {
    for (int i = 0; i < NODE_N_NEIGHBORS; i++) {
        if (neighbors[i].node == neighbor.node && neighbors[i].line == neighbor.line) {
            neighbors[i].node = nullptr;
            neighbors[i].line = nullptr;
            numNeighbors--;
            return true;
        }
    }
    return false;
}

char Node::numTrains() {
    int c = 0;
    for (int i = 0; i < NODE_N_TRAINS; i++) {
        if (trains[i] != nullptr) {
            c++; // lol, haha! funny!
        }
    }
    return c;
}

bool Node::findPath(Node* end, PathWrapper* destPath, char* destPathSize) {
    pathRequests++;
    Node* endCopy = end;

    PathCacheWrapper& cachedPath = cache.get(this, end);
    if (cachedPath.size > 0) {
        pathCacheHits++;
        std::copy(cachedPath.begin(), cachedPath.end(), destPath);
        *destPathSize = char(cachedPath.size);
        destPath[cachedPath.size] = PathWrapper();
        return true;
    }

    cachedPath = cache.get(end, this);
    if (cachedPath.size > 0) {
        pathCacheHits++;
        std::reverse_copy(cachedPath.begin(), cachedPath.end(), destPath);
        *destPathSize = char(cachedPath.size);
        for (int i = 0; i < cachedPath.size-1; i++) {
            destPath[i] = destPath[i + 1];
        }
        destPath[cachedPath.size] = PathWrapper();
        return true;
    }

    auto compare = [](Node* a, Node* b) { return a->score > b->score; };
    std::priority_queue<Node*, std::vector<Node*>, decltype(compare)> queue(compare);
    std::unordered_set<Node*> queueSet;
    std::unordered_set<Node*> visited;
    std::unordered_map<Node*, PathWrapper> from;
    std::unordered_map<Node*, float> score;

    score[this] = 0.0f;
    this->score = score[this] + dist(end) * DISTANCE_SCALE;
    queue.push(this);
    queueSet.insert(this);
    while (!queue.empty()) {
        Node* current = queue.top();
        queue.pop();
        queueSet.erase(current);

        if (current == end) {
            // path found, postprocess and return
            std::vector<PathWrapper> path;
            int numTransfers = 0;
            Line* prevLine = nullptr;
            while (from.find(end) != from.end()) {
                PathWrapper pathWrapper = from[end];
                if (true || pathWrapper.line != prevLine) {
                    prevLine = pathWrapper.line;
                    numTransfers++;
                    // would be possible to contract paths only to lines, but creates lots of issues and does not improve performance
                    // would, however, have high impact on memory
                }
                path.push_back(pathWrapper);
                end = pathWrapper.node;
            }
            path.push_back(PathWrapper{ this, path.back().line });
            std::reverse(path.begin(), path.end());
            path.push_back(PathWrapper{ endCopy, path.back().line });

            size_t pathSize = path.size();
            if (pathSize > CITIZEN_PATH_SIZE) {
                // yippee, memory safety!
                #if PATHFINDER_ERRORS == true
                std::cout << "ERR: encountered large path (" << pathSize << ") [" << this->id << " : " << end->id << " ]" << std::endl;
                #endif
                pathFails++;
                return false;
            }

            std::copy(path.begin(), path.end(), destPath);
            *destPathSize = (char)pathSize;
            destPath[pathSize] = PathWrapper();

            if (numTransfers >= CACHE_TRANSFERS_THRESHOLD) {
                cache.put(this, endCopy, destPath, pathSize);
            }

            return true;
        }

        visited.insert(current);

        for (int i = 0; i < current->numNeighbors; i++) {
            Node* neighbor = current->neighbors[i].node;
            if (neighbor == nullptr) continue;
            Line* line = current->neighbors[i].line;

            if (visited.find(neighbor) != visited.end()) continue;

            float aggregateScore = score[current] + current->weights[i];

            if (from[neighbor].line != line) {
                aggregateScore += TRANSFER_PENALTY;
            }

            if (aggregateScore < score[neighbor] || queueSet.find(neighbor) == queueSet.end()) {
                from[neighbor] = PathWrapper{ current, line };
                score[neighbor] = aggregateScore;
                neighbor->score = aggregateScore + neighbor->dist(end) * DISTANCE_SCALE;

                if (queueSet.find(neighbor) == queueSet.end()) {
                    queue.push(neighbor);
                    queueSet.insert(neighbor);
                }
            }
        }
    }
    pathFails++;
    return false; // no path found
}