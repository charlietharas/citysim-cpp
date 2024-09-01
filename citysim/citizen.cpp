#include "citizen.h"

class Node;
extern Line WALKING_LINE;

std::mutex citizenDeletionMutex;

Citizen::Citizen() {
	reset();
}

void Citizen::reset() {
	status = STATUS_SPAWNED;
	currentTrain = nullptr;
	index = 0;
	timer = 0;
}

std::string Citizen::currentPathStr() {
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
bool Citizen::updatePositionAlongPath(float speed) {
	if (index == pathSize - 1 || getNextNode() == nullptr) {
		#if ERROR_MODE == true
		std::cout << "ERR default despawned citizen @" << int(index) << std::endl;
		#endif
		status = STATUS_DESPAWNED;
		return true;
	}

	if (getCurrentNode() == nullptr) {
		#if ERROR_MODE == true
		std::cout << "ERR despawned NULLPATHREF citizen @" << int(index) << ": " << currentPathStr() << std::endl;
		#endif
		status = STATUS_DESPAWNED;
		return true;
	}

	if (timer > CITIZEN_DESPAWN_THRESH) {
		#if ERROR_MODE == true
		std::cout << "ERR despawned TIMEOUT citizen @" << int(index) << ": " << currentPathStr() << std::endl;
		#endif
		if (status == STATUS_IN_TRANSIT) {
			util::subCapacity(&currentTrain->capacity);
		}
		if (status == STATUS_AT_STOP || status == STATUS_TRANSFER) {
			util::subCapacity(&getCurrentNode()->capacity);
		}
		status = STATUS_DESPAWNED;
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

		for (int i = 0; i < NODE_N_TRAINS; i++) {
			Train* t = getCurrentNode()->trains[i];
			if (t != nullptr && t->getNextStop() == getNextNode() && t->line == getCurrentLine() && t->capacity < TRAIN_CAPACITY) {
				timer = 0;
				status = STATUS_IN_TRANSIT;
				currentTrain = t;
				currentTrain->capacity++;
				util::subCapacity(&getCurrentNode()->capacity);
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
				util::subCapacity(&currentTrain->capacity);

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

CitizenVector::CitizenVector(size_t reserve, size_t maxS) {
	vec.reserve(reserve);
	maxSize = maxS;
}

bool CitizenVector::add(Node* start, Node* end) {
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

bool CitizenVector::remove(int index) {
	vec[index].status = STATUS_DESPAWNED;

	{
		std::lock_guard<std::mutex> lock(citizenDeletionMutex);
		inactive.push(&vec[index]);
	}

	return true;
}

bool CitizenVector::triggerCitizenUpdate(int index, float speed) {
	if (vec[index].status == STATUS_DESPAWNED) {
		return true;
	}
	if (vec[index].updatePositionAlongPath(speed)) {
		remove(index);
		return true;
	}
	return false;
}