#pragma once

#include <SFML/Graphics.hpp>
#include "drawable.h"
#include "macros.h"
#include "line.h"
#include "node.h"
class Node;

class Train : public Drawable {
public:
	Train() :
		Drawable(TRAIN_MIN_SIZE, TRAIN_N_POINTS) {
	}

	char status;
	char statusForward;
	char index;
	unsigned int capacity;
	float timer;
	Line* line;

	float getDist(char indx) {
		return line->dist[indx];
	}

	Node* getStop(char indx) {
		return line->path[indx];
	}

	Node* getLastStop() {
		return line->path[index];
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

	Node* getNextStop() {
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
		float dist = (statusForward == STATUS_FORWARD) ? getDist(index) : getDist(getNextIndex());

		// 
		// acceleration/deceleration behavior?

		switch (status) {
		case STATUS_DESPAWNED:
			return;
		case STATUS_IN_TRANSIT:
			// linearly interpolate position
			if (index == line->size - 1 && statusForward == STATUS_FORWARD) statusForward = STATUS_BACK;
			if (index == 0 && statusForward == STATUS_BACK) statusForward = STATUS_FORWARD;
			nextIndex = getNextIndex();
			setPosition(getStop(index)->lerp(timer / dist, getStop(nextIndex)));

			// reached stop
			if (timer > dist) {
				getNextStop()->addTrain(this);
				goTo(getCurrentStop());
				index = nextIndex;
;				timer = 0;
				status = STATUS_AT_STOP;
			}
			break;
		case STATUS_AT_STOP:
			// done boarding/deboarding
			if (timer > TRAIN_STOP_THRESH) {
				getLastStop()->removeTrain(this);
				timer = 0;
				status = STATUS_IN_TRANSIT;
			}
			break;
		}
	}
};
