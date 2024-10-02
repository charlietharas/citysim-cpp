#pragma once

#include <iostream>
#include <mutex>
#include <stack>
#include "macros.h"
#include "util.h"
#include "train.h"
#include "line.h"

class Citizen {
public:
	float timer;
	char status;
	char index;
	char pathSize;
	char statusForward;
	float dist;
	Train* currentTrain;
	Node* currentNode;
	Line* currentLine;
	Node* nextNode;
	PathWrapper path[CITIZEN_PATH_SIZE]; // path.line[i] is used to travel between path.node[i] and path.node[i+1]

	void reset();
	std::string currentPathStr();

	inline bool moveDownPath() {
		if (++index > pathSize - 1) {
			timer = 0;
			status = STATUS_DESPAWNED;
			return true;
		}
		timer = 0;
		currentNode = path[index].node;
		currentLine = path[index].line;
		nextNode = path[index + 1].node;
		return false;
	}

	inline bool switch_WALK() {
		if (nextNode == nullptr) {
			status = STATUS_DESPAWNED;
			return true;
		}
		else {
			status = STATUS_WALK;
			dist = currentNode->dist(nextNode);
			return false;
		}
	}

	inline void switch_TRANSFER() {
		timer = 0;
		status = STATUS_TRANSFER;
		currentNode->capacity++;
		currentNode->totalRiders++;
	}

	bool updatePositionAlongPath();
	bool cull();
};

class CitizenVector {
public:
	CitizenVector(size_t reserve, size_t maxS);

	Citizen operator [](int i) const {
		return vec[i];
	}
	Citizen& operator [](int i) {
		return vec[i];
	}

	inline size_t size() {
		return vec.size();
	}
	inline size_t activeSize() {
		return vec.size() - inactive.size();
	}
	inline size_t capacity() {
		return vec.capacity();
	}
	inline size_t max() {
		return maxSize;
	}

	bool add(Node* start, Node* end);
	bool remove(int index);
private:
	size_t maxSize;
	std::vector<Citizen> vec;
	std::stack<Citizen*> inactive;
};