#include <iostream>

#include "node.h"
#include "pcw.h"

PathCache cache = PathCache(PATH_CACHE_BUCKETS, PATH_CACHE_BUCKETS_SIZE);

Node::Node() : Drawable(NODE_MIN_SIZE, NODE_N_POINTS) {
    for (int i = 0; i < N_NEIGHBORS; i++) {
        neighbors[i] = PathWrapper();
    }
    for (int i = 0; i < N_TRAINS; i++) {
        trains[i] = nullptr;
    }
}


bool Node::addTrain(Train* train) {
    for (int i = 0; i < N_TRAINS; i++) {
        if (trains[i] == nullptr) {
            trains[i] = train;
            return true;
        }
    }
    return false;
}

bool Node::removeTrain(Train* train) {
    for (int i = 0; i < N_TRAINS; i++) {
        if (trains[i] == train) {
            trains[i] = nullptr;
            return true;
        }
    }
    return false;
}

bool Node::addNeighbor(PathWrapper* neighbor, float weight) {
    for (int i = 0; i < N_NEIGHBORS; i++) {
        if (neighbors[i].node == nullptr) {
            neighbors[i] = *neighbor;
            weights[i] = weight;
            return true;
        }
    }
    return false;
}

bool Node::removeNeighbor(PathWrapper* neighbor) {
    for (int i = 0; i < N_NEIGHBORS; i++) {
        if (neighbors[i].node == neighbor->node && neighbors[i].line == neighbor->line) {
            neighbors[i] = PathWrapper({ nullptr, nullptr });
            return true;
        }
    }
    return false;
}

std::vector<PathWrapper> Node::reconstructPath(std::unordered_map<Node*, PathWrapper*>& from, Node* end) {
    std::vector<PathWrapper> path;
    while (from.find(end) != from.end()) {
        PathWrapper pathWrapper = *from[end];
        path.push_back(pathWrapper);
        end = pathWrapper.node;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

PathCacheWrapper* Node::getCachedPath(Node* end) {
    // look up path in bidirectional cache
    PathCacheWrapper* pcw = cache.get(this, end);
    if (pcw != nullptr && pcw->size > 0) {
        return pcw;
    }

    pcw = cache.get(end, this);
    if (pcw != nullptr && pcw->size > 0) {
        std::reverse(pcw->begin(), pcw->end());
        return pcw;
    }

    return nullptr;
}

bool Node::findPath(Node* end, PathWrapper* destPath, char* destPathSize) {
    PathCacheWrapper* cachedPath = getCachedPath(end);
    if (cachedPath != nullptr) {
        std::copy(cachedPath->begin(), cachedPath->end(), destPath);
        *destPathSize = char(cachedPath->size);
        if (cachedPath->size < CITIZEN_PATH_SIZE) {
            destPath[cachedPath->size] = PathWrapper();
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
            *destPathSize = (char)pathSize;
            if (pathSize < CITIZEN_PATH_SIZE) {
                destPath[pathSize] = PathWrapper();
            }

            cache.put(this, end, destPath, pathSize);

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