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
	bool justBoarded;
	Train* currentTrain;
	PathWrapper path[CITIZEN_PATH_SIZE]; // path.line[i] is used to travel between path.node[i] and path.node[i+1]

	Citizen();
	void reset();

	inline Node* getCurrentNode() {
		return path[index].node;
	}
	inline Node* getNextNode() {
		if (index + 1 > pathSize - 1) return nullptr;
		return path[index + 1].node;
	}

	inline Line* getCurrentLine() {
		return path[index].line;
	}
	inline Line* getNextLine() {
		if (index + 1 > pathSize - 1) return nullptr;
		return path[index + 1].line;
	}

	std::string currentPathStr();

	bool updatePositionAlongPath();
private:
	inline void incrCapacity() {
		getCurrentNode()->capacity++;
		getCurrentNode()->totalRiders++;
	}
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
	bool triggerCitizenUpdate(int index);
private:
	size_t maxSize;
	std::vector<Citizen> vec;
	std::stack<Citizen*> inactive;
};