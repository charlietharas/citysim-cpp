#include <iostream>
#include "node.h"
#include "pathcache.h"

extern Node nodes[MAX_NODES];

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
		trains[i] = {0, 0};
	}
}

bool Node::addTrain(TrainHandle train) {
	for (int i = 0; i < NODE_N_TRAINS; i++) {
		if (trains[i].generation == 0) {
			trains[i] = train;
			return true;
		}
	}
	return false;
}

bool Node::removeTrain(TrainHandle train) {
	for (int i = 0; i < NODE_N_TRAINS; i++) {
		if (trains[i] == train) {
			trains[i] = {0, 0};
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
		if (neighbors[i].line == nullptr) {
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
			neighbors[i].node = 0;
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
		if (trains[i].generation != 0) {
			c++;
		}
	}
	return c;
}

bool Node::findPath(Node* end, PathWrapper* destPath, char* destPathSize) {
	pathRequests++;

	// TODO: Fix path caching
	/*
    PathCacheWrapper& cachedPath = cache.get(this, end);
    if (cachedPath.size > 0) {
        pathCacheHits++;
        std::copy(cachedPath.begin(), cachedPath.end(), destPath);
        *destPathSize = char(cachedPath.size);
        
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
        return true;
    }
    */

	auto compare = [](uint16_t a, uint16_t b) { return nodes[a].score > nodes[b].score; };
	std::priority_queue<uint16_t, std::vector<uint16_t>, decltype(compare)> queue(compare);
	std::unordered_set<uint16_t> queueSet;
	std::unordered_set<uint16_t> visited;
	std::unordered_map<uint16_t, PathWrapper> from;
	std::unordered_map<uint16_t, float> score;

	uint16_t start_id = this->numerID;
	uint16_t end_id = end->numerID;

	score[start_id] = 0.0f;
	nodes[start_id].score = score[start_id] + this->dist(end) * DISTANCE_SCALE;
	queue.push(start_id);
	queueSet.insert(start_id);

	while (!queue.empty()) {
		uint16_t current_id = queue.top();
		queue.pop();
		queueSet.erase(current_id);

		if (current_id == end_id) {
			// path found, postprocess and return
			std::vector<PathWrapper> path;
			uint16_t current_path_node = end_id;
			while (from.find(current_path_node) != from.end()) {
				PathWrapper pathWrapper = from[current_path_node];
				path.push_back({current_path_node, pathWrapper.line});
				current_path_node = pathWrapper.node;
			}
			path.push_back({start_id, path.back().line});
			std::reverse(path.begin(), path.end());

			size_t pathSize = path.size();
			if (pathSize > CITIZEN_PATH_SIZE) {
				pathFails++;
				return false;
			}

			std::copy(path.begin(), path.end(), destPath);
			*destPathSize = (char)pathSize;

			// TODO: Fix path caching
			//cache.put(this, end, destPath, pathSize);

			return true;
		}

		visited.insert(current_id);
		Node& current_node = nodes[current_id];

		for (int i = 0; i < current_node.numNeighbors; i++) {
			uint16_t neighbor_id = current_node.neighbors[i].node;
			Line* line = current_node.neighbors[i].line;

			if (visited.find(neighbor_id) != visited.end()) continue;

			float aggregateScore = score[current_id] + current_node.weights[i];

			if (from.count(neighbor_id) && from[neighbor_id].line != line) {
				aggregateScore += TRANSFER_PENALTY;
			}

			if (!score.count(neighbor_id) || aggregateScore < score[neighbor_id]) {
				from[neighbor_id] = {current_id, line};
				score[neighbor_id] = aggregateScore;
				nodes[neighbor_id].score = aggregateScore + nodes[neighbor_id].dist(end) * DISTANCE_SCALE;

				if (queueSet.find(neighbor_id) == queueSet.end()) {
					queue.push(neighbor_id);
					queueSet.insert(neighbor_id);
				}
			}
		}
	}
	pathFails++;
	return false; // no path found
}