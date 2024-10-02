#include "train.h"

// aah, this whole class is so confusing! why did i do this?
Train::Train() {
	 Drawable(TRAIN_MIN_SIZE, TRAIN_N_POINTS);
}

Node* Train::getLastStop() {
	return getStop(index);
}

Node* Train::getCurrentStop() {
	if (status == STATUS_IN_TRANSIT) {
		return getNextStop();
	}
	else if (status == STATUS_AT_STOP) {
		return getLastStop();
	}
	return nullptr;
}

Node* Train::getNextStop() {
	return line->path[getNextIndex()];
}

int Train::getNextIndex(bool reversed) {
	int increment = (statusForward == STATUS_FORWARD) ? 1 : -1;
	if (reversed) increment *= -1;
	if (index + increment < 0 || index + increment >= line->size) {
		increment *= -1;
	}
	return index + increment;
}

int Train::getPrevIndex() {
	return getNextIndex(true);
}

int Train::getCorrectNextIndex() {
	if (statusForward == STATUS_FORWARD) {
		return getNextIndex();
	}
	else {
		return getNextIndex(true);
	}
}

void Train::updatePositionAlongLine() {
	timer += TRAIN_SPEED;

	switch (status) {
	case STATUS_DESPAWNED:
		return;
	case STATUS_TRANSFER:
		if (statusForward == STATUS_FORWARD && index == line->size - 1) statusForward = STATUS_BACKWARD;
		if (statusForward == STATUS_BACKWARD && index == 0) statusForward = STATUS_FORWARD;

		if (statusForward == STATUS_FORWARD) {
			dist = getDist(index);
		}
		else {
			dist = getDist(getNextIndex());
		}
		nextIndex = getNextIndex();

		status = STATUS_IN_TRANSIT;
		break;
	case STATUS_IN_TRANSIT:
		// linearly interpolate position
		setPosition(getStop(index)->lerp(timer / dist, getStop(nextIndex)));

		// reached stop
		if (timer > dist) {
			if (getNextStop()->addTrain(this)) {
				goTo(getCurrentStop());
				index = nextIndex;
				timer = 0;
				status = STATUS_AT_STOP;
			}
			#if TRAIN_ERRORS == true
			else {
				std::cout << "ERR: failed to add [" << line->id << "] train to " << getNextStop()->id << std::endl;
			}
			#endif
		}
		break;
	case STATUS_AT_STOP:
		// done boarding/deboarding
		if (timer > TRAIN_STOP_THRESH) {
			if (getLastStop()->removeTrain(this)) {
				timer = 0;
				status = STATUS_TRANSFER;
			}
			#if TRAIN_ERRORS == true
			else {
				std::cout << "ERR: failed to remove [" << line->id << "] train from " << getLastStop()->id << std::endl;
			}
			#endif
		}
		break;
	}
}