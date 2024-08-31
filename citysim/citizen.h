#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <mutex>
#include "drawable.h"
#include "macros.h"
class Node;

extern Line WALKING_LINE;

std::mutex citizenDeletionMutex;

class Citizen {
public:
	Citizen() {
		reset();
	}

	float timer;
	char status;
	char index;
	Train* currentTrain;

	char pathSize;
	PathWrapper path[CITIZEN_PATH_SIZE];

	void reset() {
		status = STATUS_SPAWNED;
		currentTrain = nullptr;
		index = 0;
		timer = 0;
	}

	inline Node* getCurrentNode() {
		return path[index].node;
	}

	inline Node* getNextNode() {
		if (index + 1 >= pathSize - 1) return nullptr;
		return path[index + 1].node;
	}

	inline Line* getCurrentLine() {
		return path[index].line;
	}

	inline Line* getNextLine() {
		if (index + 1 >= pathSize - 1) return nullptr;
		return path[index + 1].line;
	}

	std::string currentPathStr() {
		char sum[NODE_ID_SIZE * 2 + LINE_ID_SIZE * 2 + 7];
		std::strcpy(sum, getCurrentNode()->id);
		std::strcat(sum, ",");
		std::strcat(sum, getCurrentLine()->id);
		std::strcat(sum, "->");
		std::strcat(sum, getNextNode()->id);
		std::strcat(sum, ",");
		std::strcat(sum, getNextLine()->id);
		return sum;
	}

	// returns true if the citizen has been despawned/is despawned
	bool updatePositionAlongPath(float speed) {
		if (index == pathSize - 1 || getNextNode() == nullptr) {
			// std::cout << "ERR default despawned citizen @" << int(index) << std::endl;
			status = STATUS_DESPAWNED_ERR;
			return true;
		}

		if (getCurrentNode() == nullptr) {
			// std::cout << "ERR despawned NULLPATHREF citizen @" << int(index) << ": " << currentPathStr() << std::endl;
			status = STATUS_DESPAWNED_ERR;
			return true;
		}

		if (timer > CITIZEN_DESPAWN_THRESH) {
			// std::cout << "ERR despawned TIMEOUT citizen @" << int(index) << ": " << currentPathStr() << std::endl;
			if (status == STATUS_IN_TRANSIT) {
				subCapacity(&currentTrain->capacity);
			}
			if (status == STATUS_AT_STOP || status == STATUS_TRANSFER) {
				subCapacity(&getCurrentNode()->capacity);
			}
			status = STATUS_DESPAWNED_ERR;
			return true;
		}

		timer += speed;
		float dist;

		switch (status) {
		case STATUS_DESPAWNED:
			return true;
		case STATUS_SPAWNED:
			if (getCurrentLine() == &WALKING_LINE) {
				status = STATUS_WALK;
			}
			else {
				status = STATUS_TRANSFER;
				getCurrentNode()->capacity++;
			}
			return false;
		case STATUS_WALK:
			dist = getCurrentNode()->dist(getNextNode());
			if (timer > dist) {
				timer = 0;
				if (getNextLine() == &WALKING_LINE) {
					status = STATUS_WALK;
				}
				else {
					status = STATUS_TRANSFER;
					getCurrentNode()->capacity++;
				}
				index++;
			}
			return false;
		case STATUS_TRANSFER:
			if (timer > CITIZEN_TRANSFER_THRESH) {
				timer = 0;
				status = STATUS_AT_STOP;
			}
			return false;
		case STATUS_AT_STOP:
			if (getNextLine() == &WALKING_LINE) {
				status = STATUS_WALK;
				return false;
			}

			for (int i = 0; i < N_TRAINS; i++) {
				Train* t = getCurrentNode()->trains[i];
				if (t != nullptr && t->getNextStop() == getNextNode() && t->line == getCurrentLine() && t->capacity < TRAIN_CAPACITY) {
					timer = 0;
					status = STATUS_IN_TRANSIT;
					currentTrain = t;
					currentTrain->capacity++;
					subCapacity(&getCurrentNode()->capacity);
					index++;
					justBoarded = true;
				}

			}
			return false;
		case STATUS_IN_TRANSIT:
			if (justBoarded && currentTrain->status == STATUS_IN_TRANSIT) justBoarded = false;
			if (!justBoarded && currentTrain->status == STATUS_AT_STOP && currentTrain->getCurrentStop() == getCurrentNode()) {
				timer = 0;
				index++;

				if (index == pathSize - 1) {
					status = STATUS_DESPAWNED;
					return true;
				}

				if (getCurrentLine() != currentTrain->line) {
					subCapacity(&currentTrain->capacity);

					if (getCurrentLine() == &WALKING_LINE) {
						status = STATUS_WALK;
					}
					else {
						status = STATUS_TRANSFER;
						getCurrentNode()->capacity++;
					}

					currentTrain = nullptr;

				}
			}
			return false;
		default:
			return (status == STATUS_DESPAWNED);
		}
		}
private:
	bool justBoarded;
};

class CitizenVector {
public:
	CitizenVector(size_t reserve, size_t maxS) {
		vec.reserve(reserve);
		maxSize = maxS;
	}
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
	bool add(Node* start, Node* end) {
		if (inactive.empty()) {
			if (size() > maxSize) {
				return false;
			}
			Citizen c = Citizen();
			if (!start->findPath(end, c.path, &c.pathSize)) {
				return false;
			}
			c.reset();
			vec.push_back(c);
		}
		else {
			std::lock_guard<std::mutex> lock(citizenDeletionMutex);
			Citizen* c = inactive.top();
			if (!start->findPath(end, c->path, &c->pathSize)) {
				return false;
			}
			inactive.pop();
			c->reset();
		}
		return true;
	}
	bool remove(int index) {
		vec[index].status = STATUS_DESPAWNED;
		
		{
			std::lock_guard<std::mutex> lock(citizenDeletionMutex);
			inactive.push(&vec[index]);
		}

		return true;
	}
	bool triggerCitizenUpdate(int index, float speed) {
		if (vec[index].status == STATUS_DESPAWNED) {
			return true;
		}
		if (vec[index].updatePositionAlongPath(speed)) {
			remove(index);
			return true;
		}
		return false;
	}
private:
	std::vector<Citizen> vec;
	std::stack<Citizen*> inactive; // would this be faster/safer as a deque or other data structure?
	size_t maxSize;
};