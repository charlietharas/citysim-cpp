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
#include "pathCache.h"

class Train;

struct PathWrapper {
	Node* node;
	Line* line;
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
				neighbors[i] = PathWrapper({nullptr, nullptr});
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

	bool findPath(Node* end, PathWrapper* destPath, char* destPathSize) {
		// check cache
		PathCacheWrapper pcw = cache.get(NodePairWrapper(this, end));
		if (pcw.pathSize > 0) {
			for (size_t i = 0; i < pcw.pathSize; i++) {
				destPath[i] = pcw.path[i];
			}
			*destPathSize = pcw.pathSize;
			return true;
		}
		
		// cache is bidirectional, reverse the path if it has already been discovered
		// TODO check if this messes shit up
		pcw = cache.get(NodePairWrapper(end, this));
		if (pcw.pathSize > 0) {
			for (size_t i = 0; i < pcw.pathSize; i++) {
				destPath[i] = pcw.path[pcw.pathSize-i-1];
			}
			*destPathSize = pcw.pathSize;
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
			queueSet.erase(this);

			// path found
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

				// copy path
				for (size_t i = 0; i < pathSize; i++) {
					destPath[i] = path[i];
				}
				*destPathSize = pathSize;

				// place path in cache
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

		return false; // no path
	}
};
