#include "train.h"

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

void Train::updatePositionAlongLine(float speed) {
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
			#if ERROR_MODE == true
			else {
				std::cout << "ERR failed to add [" << line->id << "] train to " << getNextStop()->id << std::endl;
			}
			#endif
		}
		break;
	case STATUS_AT_STOP:
		// done boarding/deboarding
		if (timer > TRAIN_STOP_THRESH) {
			if (getLastStop()->removeTrain(this)) {
				timer = 0;
				status = STATUS_IN_TRANSIT;
			}
			#if ERROR_MODE == true
			else {
				std::cout << "ERR removing [" << line->id << "] train from " << getLastStop()->id << std::endl;
			}
			#endif
		}
		break;
	}
}