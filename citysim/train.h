#pragma once

#include <SFML/Graphics.hpp>
#include "drawable.h"
#include "macros.h"
#include "line.h"
#include "node.h"
class Node;

class Train : public Drawable {
public:
	Train() {
		Drawable(TRAIN_MIN_SIZE, TRAIN_N_POINTS);
		status = STATUS_DESPAWNED;
		statusForward = STATUS_DESPAWNED;
		index = 0;
		capacity = 0;
		timer = 0;
		line = nullptr;
	}

	char status;
	char statusForward;
	char index;
	unsigned int capacity;
	float timer;
	Line* line;

	inline float getDist(char indx) {
		return line->dist[indx];
	}

	inline Node* getStop(char indx) {
		return line->path[indx];
	}

	inline Node* getLastStop() {
		return getStop(index);
	}

	Node* getCurrentStop() {
		if (status == STATUS_IN_TRANSIT) {
			return getNextStop();
		}
		else if (status == STATUS_AT_STOP) {
			return getLastStop();
		}
		return nullptr;
	}

	inline Node* getNextStop() {
		return line->path[getNextIndex()];
	}

	int getNextIndex(bool reversed = false) {
		int increment = (statusForward == STATUS_FORWARD) ? 1 : -1;
		if (reversed) increment *= -1;
		if (index + increment < 0 || index + increment >= line->size) {
			increment *= -1;
		}
		return index + increment;
	}

	int getPrevIndex() {
		return getNextIndex(true);
	}

	void updatePositionAlongLine(float speed) {
		timer += speed;
		char nextIndex = 0;
		float dist;

		switch (status) {
		case STATUS_DESPAWNED:
			return;
		case STATUS_IN_TRANSIT:
			if (statusForward == STATUS_FORWARD) {
				dist = getDist(index);
			}
			else {
				dist = getDist(getNextIndex());
			}

			// linearly interpolate position
			if (statusForward == STATUS_FORWARD && index == line->size - 1) statusForward = STATUS_BACK;
			if (statusForward == STATUS_BACK && index == 0) statusForward = STATUS_FORWARD;
			nextIndex = getNextIndex();
			setPosition(getStop(index)->lerp(timer / dist, getStop(nextIndex)));

			// reached stop
			if (timer > dist) {
				if (getNextStop()->addTrain(this)) {
					goTo(getCurrentStop());
					index = nextIndex;
					timer = 0;
					status = STATUS_AT_STOP;
				}
				else {
					std::cout << "ERR failed to add [" << line->id << "] train to " << getNextStop()->id << std::endl;
				}
			}
			break;
		case STATUS_AT_STOP:
			// done boarding/deboarding
			if (timer > TRAIN_STOP_THRESH) {
				if (getLastStop()->removeTrain(this)) {
					timer = 0;
					status = STATUS_IN_TRANSIT;
					if (capacity == TRAIN_CAPACITY) {
						// std::cout << "Full [" << line->id << "] train leaving " << getLastStop()->id << std::endl;
					}
				}
				else {
					std::cout << "ERR removing [" << line->id << "] train from " << getLastStop()->id << std::endl;
				}
			}
			break;
		}
	}
};
