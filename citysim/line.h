#pragma once

#include <SFML/Graphics.hpp>

class Node;

struct Line {
public:
	char size;
	char id[LINE_ID_SIZE];
	sf::Color color;
	Node* path[64];
	float dist[64]; // dist[i] is equal to the distance between path[i] and path[i+1]
};
