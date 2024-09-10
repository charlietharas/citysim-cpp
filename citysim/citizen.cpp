#include "citizen.h"

class Node;
extern Line WALKING_LINE;

std::mutex blockStack; // controls access to CitizenVector.inactive()
std::mutex citizensMutex; // controls access to citizens.vec (used for debug reports, simulation, pushing back new citizens)

#define MOVE if (moveDownPath()) return true

void Citizen::reset() {
	status = STATUS_SPAWNED;
	currentTrain = nullptr;
	currentNode = path[0].node;
	currentLine = path[0].line;
	nextNode = path[1].node;
	index = 0;
	timer = 0;
	dist = 0;
}

std::string Citizen::currentPathStr() {
	char sum[NODE_ID_SIZE * 2 + LINE_ID_SIZE * 2 + 7];
	std::strcpy(sum, currentNode->id);
	std::strcat(sum, ",");
	std::strcat(sum, currentLine->id);
	std::strcat(sum, "->");
	if (nextNode != nullptr) {
		std::strcat(sum, nextNode->id);
	}
	else {
		std::strcat(sum, "NEXT_NODE_NULL");
	}
	return sum;
}

// returns true if the citizen has been despawned/is despawned
bool Citizen::updatePositionAlongPath() {
	if (nextNode == nullptr) {
		#if CITIZEN_SPAWN_ERRORS == true
		std::cout << "ERR: despawned NULLPATHREF_MISC citizen@" << int(index) << ": " << currentPathStr() << std::endl;
		#endif
		status = STATUS_DESPAWNED;
		return true;
	}

	timer += CITIZEN_SPEED;

	switch (status) {
	case STATUS_DESPAWNED:
		return true;

	case STATUS_SPAWNED:
		if (currentLine == &WALKING_LINE) {
			switch_WALK();
		}
		else {
			switch_TRANSFER();
		}
		return false;

	case STATUS_WALK:
		if (timer > dist) {
			MOVE;
			if (currentLine == &WALKING_LINE) {
				switch_WALK();
			}
			else {
				switch_TRANSFER();
			}
		}
		return false;

	case STATUS_TRANSFER:
		if (timer > CITIZEN_TRANSFER_THRESH) {
			timer = 0;
			int currentInd, nextInd;
			for (int i = 0; i < currentLine->size; i++) {
				Node* n = currentLine->path[i];
				if (n == currentNode) {
					currentInd = i;
				}
				if (n == nextNode) {
					nextInd = i;
				}
			}
			statusForward = nextInd > currentInd ? STATUS_FORWARD : STATUS_BACKWARD;
			status = STATUS_AT_STOP;
		}
		return false;

	case STATUS_AT_STOP:
		for (int i = 0; i < currentNode->numTrains(); i++) {
			Train* t = currentNode->trains[i];
			if (t != nullptr && t->statusForward == statusForward && t->line == currentLine && t->capacity < TRAIN_CAPACITY) {
				util::subCapacity(&currentNode->capacity);
				MOVE;
				status = STATUS_BOARDED;
				currentTrain = t;
				currentTrain->capacity++;
			}
		}
		return false;

	case STATUS_BOARDED:
		if (currentTrain->status == STATUS_IN_TRANSIT) {
			status = STATUS_IN_TRANSIT;
		}
		return false;

	case STATUS_IN_TRANSIT:
		if (currentTrain->status == STATUS_AT_STOP && currentTrain->getCurrentStop() == currentNode) {
			if (moveDownPath()) {
				util::subCapacity(&currentTrain->capacity);
				return true;
			}

			if (currentLine != currentTrain->line) {
				util::subCapacity(&currentTrain->capacity);

				if (currentLine == &WALKING_LINE) {
					switch_WALK();
				}
				else {
					switch_TRANSFER();
				}

				currentTrain = nullptr;
			}
		}
		return false;

	default:
		return (status == STATUS_DESPAWNED);
	}
}

bool Citizen::cull() {
	if (timer > CITIZEN_DESPAWN_THRESH) {
		#if CITIZEN_SPAWN_ERRORS == true
		std::cout << "ERR: despawned TIMEOUT citizen @" << int(index) << ": " << currentPathStr() << std::endl;
		#endif
		if (status == STATUS_IN_TRANSIT) {
			util::subCapacity(&currentTrain->capacity);
		}
		if (status == STATUS_AT_STOP || status == STATUS_TRANSFER) {
			util::subCapacity(&currentNode->capacity);
		}
		status = STATUS_DESPAWNED;
		return true;
	}
	return false;
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
		c.reset();
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
			{
				std::lock_guard<std::mutex> stackLock(blockStack);
				inactive.push(c);
			}
			return false;
		}

		c->reset();
	}
	return true;
}

bool CitizenVector::remove(int index) {
	inactive.push(&vec[index]);
	return true;
}
