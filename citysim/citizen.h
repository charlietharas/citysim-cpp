#pragma once

#include <SFML/Graphics.hpp>
#include "drawable.h"
#include "macros.h"
#include "pathCache.h"
class Node;

class Citizen : public Drawable {
public:
	Citizen() :
		Drawable(CITIZEN_SIZE, CITIZEN_N_POINTS) {
		sf::CircleShape::setFillColor(sf::Color::Black);
	}

	float timer;
	char status;
	char index;
	Train* currentTrain;
	char pathSize;
	PathWrapper path[CITIZEN_PATH_SIZE];

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
	// TODO fix capacity updates for currentNode/currentTrain (& improve rendering)
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
				if (getCurrentNode()->capacity > 0) {
					getCurrentNode()->capacity--;
				}
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
					if (getCurrentNode()->capacity > 0) {
						getCurrentNode()->capacity--;
					}
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
					if (currentTrain != nullptr) currentTrain->capacity--;

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
