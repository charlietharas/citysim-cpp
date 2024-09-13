#pragma once

#include <SFML/Graphics.hpp>
#include <iostream>
#include "drawable.h"
#include "macros.h"
#include "line.h"
#include "node.h"
class Node;

class Train : public Drawable {
public:
	Train();

	char status;
	char statusForward;
	char index;
	char nextIndex;
	unsigned int capacity;
	float timer;
	float dist;
	Line* line;

	inline float getDist(char indx) {
		return line->dist[indx];
	}

	inline Node* getStop(char indx) {
		return line->path[indx];
	}
	Node* getLastStop();
	Node* getCurrentStop();
	Node* getNextStop();

	int getNextIndex(bool reversed = false);
	int getPrevIndex();

	void updatePositionAlongLine();
};
