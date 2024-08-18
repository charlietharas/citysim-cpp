#pragma once

#include <SFML/Graphics.hpp>
#include "drawable.h"
#include "macros.h"
class Node;

extern Line WALKING_LINE;

class Citizen : public Drawable {
public:
	Citizen() {
		Drawable(CITIZEN_SIZE, CITIZEN_N_POINTS);
		sf::CircleShape::setFillColor(sf::Color::Black);
		clearNonPath();
	}

	float timer;
	char status;
	char index;
	Train* currentTrain;

	char pathSize;
	PathWrapper path[CITIZEN_PATH_SIZE];

	inline void clearNonPath() {
		status = STATUS_SPAWNED;
		currentTrain = nullptr;
		index = 0;
		timer = 0;
	}

	Node* getCurrentNode() {
		return path[index].node;
	}

	Node* getNextNode() {
		if (index + 1 >= pathSize - 1) return nullptr;
		return path[index + 1].node;
	}

	Line* getCurrentLine() {
		return path[index].line;
	}

	Line* getNextLine() {
		if (index + 1 >= pathSize - 1) return nullptr;
		return path[index + 1].line;
	}

	// returns true if the citizen has been despawned/is despawned
	bool updatePositionAlongPath(float speed) {
		timer += speed;
		float dist;

		if (index == pathSize - 1) {
			status = STATUS_DESPAWNED;
			return true;
		}

		if (getCurrentNode() == nullptr || getCurrentLine() == nullptr) {
			status = STATUS_DESPAWNED_ERR;
			return true;
		}

		// TODO actually handle these errors instead of just despawning/ignoring them
		if (timer > CITIZEN_DESPAWN_THRESH) {
			if (status == STATUS_AT_STOP) {
				getCurrentNode()->capacity = std::min(getCurrentNode()->capacity-1, unsigned int(0));
			}
			status = STATUS_DESPAWNED_ERR;
			return true;
		}

		switch (status) {
		case STATUS_DESPAWNED:
			return true;
		case STATUS_SPAWNED:
			if (getCurrentLine() == &WALKING_LINE) {
				status = STATUS_WALK;
			} else {
				status = STATUS_TRANSFER;
				getCurrentNode()->capacity++;
			}
		case STATUS_WALK:
			// this is kind of janky
			if (getNextNode() == nullptr) {
				status = STATUS_DESPAWNED;
				return true;
			}

			dist = getCurrentNode()->dist(getNextNode());
			setPosition(getCurrentNode()->lerp(dist, getNextNode()));
			if (timer > dist) {
				timer = 0;
				if (getNextLine() == &WALKING_LINE) {
					status = STATUS_WALK;
				}
				else {
					status = STATUS_TRANSFER;
				}
				index++;
			}
			return false;
		case STATUS_TRANSFER:
			if (timer > CITIZEN_TRANSFER_THRESH) {
				timer = 0;
				goTo(getCurrentNode());
				status = STATUS_AT_STOP;
			}
			return false;
		// some citizens definitely get stuck here
		case STATUS_AT_STOP:
			for (int i = 0; i < N_TRAINS; i++) {
				Train* t = getCurrentNode()->trains[i];
				if (t != nullptr && t->getNextStop() == getNextNode() && t->line == getCurrentLine() && t->capacity < TRAIN_CAPACITY) {
					timer = 0;
					status = STATUS_IN_TRANSIT;
					currentTrain = t;
					currentTrain->capacity++;
					getCurrentNode()->capacity = std::min(getCurrentNode()->capacity-1, unsigned int(0));
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
					currentTrain->capacity = std::min(currentTrain->capacity, unsigned int(0));

					if (getCurrentLine() == &WALKING_LINE) {
						status = STATUS_WALK;
					} else {
						status = STATUS_TRANSFER;
						getCurrentNode()->capacity++;
					}

					goTo(getCurrentNode());
					currentTrain = nullptr;
						
				}
			} else {
				goTo(currentTrain);
			}
			return false;
		}
		return (status == STATUS_DESPAWNED);
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
	size_t size() {
		return vec.size();
	}
	size_t activeSize() {
		return vec.size() - inactive.size();
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
			c.clearNonPath();
			vec.push_back(c);
		}
		else {
			Citizen* c = inactive.top();
			if (!start->findPath(end, c->path, &c->pathSize)) {
				return false;
			}
			inactive.pop();
			c->clearNonPath();
		}
		return true;
	}
	bool remove(int i) {
		if (i > size() - 1 || i < 0 || size() == 0) {
			return false;
		}
		if (vec[i].status == STATUS_DESPAWNED) {
			return false;
		}
		vec[i].status = STATUS_DESPAWNED;
		inactive.push(&vec[i]);
		return true;
	}
	void shrink() {
		// this code is horrendous!
		// TODO: swap despawned citizens to the end to batch .erase()
		std::vector<int> toRemove;
		for (int i = 0; i < size(); i++) {
			if (vec[i].status == STATUS_DESPAWNED) {
				toRemove.push_back(i);
			}
		}
		int decrement = 0;
		for (int i : toRemove) {
			vec.erase(vec.begin() + i - decrement++);
		}
		vec.shrink_to_fit();
	}
	bool triggerCitizenUpdate(int i, float speed) {
		if (vec[i].status == STATUS_DESPAWNED) {
			return true;
		}
		if (vec[i].updatePositionAlongPath(speed)) {
			remove(i);
			return true;
		}
		return false;
	}
private:
	std::vector<Citizen> vec;
	std::stack<Citizen*> inactive;
	size_t maxSize;
};