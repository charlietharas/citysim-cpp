#include "citizen.h"

class Node;
extern Line WALKING_LINE;

std::mutex blockStack; // controls access to CitizenVector.inactive()
std::mutex citizensMutex; // controls access to citizens.vec (used for debug reports, simulation, pushing back new citizens)

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
	if (getNextNode() != nullptr) {
		std::strcat(sum, getNextNode()->id);
		std::strcat(sum, ",");
		std::strcat(sum, getNextLine()->id);
	}
	else {
		std::strcat(sum, "NEXT_NODE_NULL");
	}
	return sum;
}

// returns true if the citizen has been despawned/is despawned
bool Citizen::updatePositionAlongPath() {
	if (getNextNode() == nullptr) {
		#if CITIZEN_SPAWN_ERRORS == true
		std::cout << "ERR: despawned NULLPATHREF_MISC citizen@" << int(index) << ": " << currentPathStr() << std::endl;
		#endif
		status = STATUS_DESPAWNED;
		return true;
	}

	if (timer > CITIZEN_DESPAWN_THRESH) {
		#if CITIZEN_SPAWN_ERRORS == true
		std::cout << "ERR: despawned TIMEOUT citizen @" << int(index) << ": " << currentPathStr() << std::endl;
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

	timer += CITIZEN_SPEED;
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
			incrCapacity();
		}
		return false;

	case STATUS_WALK:
		dist = getCurrentNode()->dist(getNextNode());
		if (timer > dist) {
			index++;
			if (index == pathSize - 1) {
				status = STATUS_DESPAWNED;
				return true;
			}
			timer = 0;
			if (getCurrentLine() == &WALKING_LINE) {
				status = STATUS_WALK;
			}
			else {
				status = STATUS_TRANSFER;
				incrCapacity();
			}

		}
		return false;

	case STATUS_TRANSFER:
		if (timer > CITIZEN_TRANSFER_THRESH) {
			timer = 0;
			status = STATUS_AT_STOP;
		}
		return false;

	case STATUS_AT_STOP:
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

			if (getCurrentLine() != currentTrain->line) {
				util::subCapacity(&currentTrain->capacity);

				if (getCurrentLine() == &WALKING_LINE) {
					status = STATUS_WALK;
				}
				else {
					status = STATUS_TRANSFER;
					incrCapacity();
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
	if (inactive.size() < NUM_CITIZEN_WORKER_THREADS) {
		if (size() > maxSize) {
			return false;
		}
		Citizen c = Citizen();
		if (!start->findPath(end, c.path, &c.pathSize)) {
			return false;
		}
		{
			std::lock_guard<std::mutex> citizensLock(citizensMutex);
			vec.push_back(c);
		}
	}
	else {
		Citizen* c;
		{
			std::lock_guard<std::mutex> stackLock(blockStack);
			c = inactive.top();
			inactive.pop();
		}
		if (c == nullptr) {
			#if CITIZEN_SPAWN_ERRORS == true
			std::cout << "ERR: found nullptr reference in CitizenVector stack" << std::endl;
			#endif
			return false;
		}
		if (!start->findPath(end, c->path, &c->pathSize)) {
			std::lock_guard<std::mutex> stackLock(blockStack);
			inactive.push(c);
			return false;
		}

		c->reset();
	}
	return true;
}

bool CitizenVector::remove(int index) {
	vec[index].status = STATUS_DESPAWNED;
	{
		std::lock_guard<std::mutex> stackLock(blockStack);
		inactive.push(&vec[index]);
	}
	return true;
}

bool CitizenVector::triggerCitizenUpdate(int index) {
	if (vec[index].status == STATUS_DESPAWNED) {
		return true;
	}
	if (vec[index].updatePositionAlongPath()) {
		remove(index);
		return true;
	}
	return false;
}
