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

std::vector<PathWrapper> Node::bidirectionalAStar(Node* start, Node* end) {
    std::unordered_map<Node*, float> forwardDist, backwardDist;
    std::unordered_map<Node*, PathWrapper> forwardPrev, backwardPrev;
    auto compare = [](const std::pair<float, Node*>& a, const std::pair<float, Node*>& b) {
        return a.first > b.first;
    };
    std::priority_queue<std::pair<float, Node*>, std::vector<std::pair<float, Node*>>, decltype(compare)> forwardPQ(compare), backwardPQ(compare);

    forwardDist[start] = 0;
    backwardDist[end] = 0;
    float dist = start->dist(end);
    forwardPQ.push({ dist, start});
    backwardPQ.push({ dist, end});

    float bestDist = FLT_MAX;
    Node* meetingNode = nullptr;

    // multithread two separate while loops 
    while (!forwardPQ.empty() && !backwardPQ.empty()) {
        // forward search
        auto forwardTop = forwardPQ.top();
        float forwardScore = forwardTop.first;
        Node* forwardNode = forwardTop.second;
        forwardPQ.pop();

        float forwardDistToNode = forwardDist[forwardNode];
        if (forwardDistToNode > bestDist) break;

        if (backwardDist.count(forwardNode)) {
            float totalDist = forwardDistToNode + backwardDist[forwardNode];
            if (totalDist < bestDist) {
                bestDist = totalDist;
                meetingNode = forwardNode;
            }
        }

        for (int i = 0; i < forwardNode->numNeighbors; i++) {
            PathWrapper& wrapper = forwardNode->neighbors[i];
            Node* node = wrapper.node;
            if (node->level < forwardNode->level) continue;
            float newDist = forwardDistToNode + forwardNode->dist(node);
            if (!forwardDist.count(node) || newDist < forwardDist[node]) {
                forwardDist[node] = newDist;
                forwardPrev[node] = { forwardNode, wrapper.line };
                float score = newDist + node->dist(end);
                forwardPQ.push({ score, node });
            }
        }

        // backward search
        auto backwardTop = backwardPQ.top();
        float backwardScore = backwardTop.first;
        Node* backwardNode = backwardTop.second;
        backwardPQ.pop();

        float backwardDistToNode = backwardDist[backwardNode];
        if (backwardDistToNode > bestDist) break;

        if (forwardDist.count(backwardNode)) {
            float totalDist = backwardDistToNode + forwardDist[backwardNode];
            if (totalDist < bestDist) {
                bestDist = totalDist;
                meetingNode = backwardNode;
            }
        }

        for (int i = 0; i < backwardNode->numNeighbors; i++) {
            PathWrapper& wrapper = backwardNode->neighbors[i];
            Node* node = wrapper.node;
            if (node->level < backwardNode->level) continue;
            float newDist = backwardDistToNode + backwardNode->dist(node);
            if (!backwardDist.count(node) || newDist < backwardDist[node]) {
                backwardDist[node] = newDist;
                backwardPrev[node] = { backwardNode, wrapper.line };
                float score = newDist + node->dist(start);
                backwardPQ.push({ score, node });
            }
        }

    }

    if (!meetingNode) return {};

    // reconstruct path by meeting in the middle
    std::vector<PathWrapper> path;
    Node* current = meetingNode;
    while (current != start) {
        path.push_back(forwardPrev[current]);
        current = forwardPrev[current].node;
    }
    std::reverse(path.begin(), path.end());
    
    int revInd = path.size();
    path.push_back({ meetingNode, nullptr });

    current = meetingNode;
    while (current != end) {
        path.push_back(backwardPrev[current]);
        current = backwardPrev[current].node;
    }

    path[revInd].line = path[revInd - 1].line;

    return path;
}

bool Node::findPath(Node* end, PathWrapper* destPath, char* destPathSize) {
    pathRequests++;
    PathCacheWrapper& cachedPath = cache.get(this, end);
    if (cachedPath.size > 0) {
        std::copy(cachedPath.begin(), cachedPath.end(), destPath);
        *destPathSize = char(cachedPath.size);
        pathCacheHits++;
        return true;
    }

    std::vector<PathWrapper> path = bidirectionalAStar(this, end);

    if (path.empty()) return false;

    size_t pathSize = path.size();
    if (pathSize > CITIZEN_PATH_SIZE) {
        #if PATHFINDER_ERRORS == true
        std::cout << "ERR: pathfinder encountered large path (" << pathSize << ") [" << this->id << " : " << end->id << " ]" << std::endl;
        #endif
        pathFails++;
        return false;
    }

    std::reverse_copy(path.begin(), path.end(), destPath);
    *destPathSize = (char)pathSize;

    // TODO restrict cache puts to (subjective) more complex paths
    cache.put(this, end, destPath, pathSize, false);
    cache.put(end, this, destPath, pathSize, true);

    return true;
}