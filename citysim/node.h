#pragma once

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <queue>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "macros.h"
#include "drawable.h"
#include "line.h"

class Train;
class PathCacheWrapper;

struct PathWrapper {
    Node* node;
    Line* line;
};

class Node : public Drawable {
public:
    char id[NODE_ID_SIZE];
    unsigned int ridership;
    unsigned int capacity;
    unsigned short int numerID;
    char numNeighbors;
    char status;
    float score;
    unsigned short int gridPos;
    PathWrapper neighbors[NODE_N_NEIGHBORS];
    float weights[NODE_N_NEIGHBORS];
    Train* trains[NODE_N_TRAINS];

    Node();

    bool addTrain(Train* train);
    bool removeTrain(Train* train);
    bool addNeighbor(PathWrapper& neighbor, float weight);
    bool removeNeighbor(PathWrapper& neighbor);

    void setGridPos(char x, char y);
    char gridX();
    char gridY();
    char lowerGridX();
    char upperGridX();
    char lowerGridY();
    char upperGridY();
    unsigned int numTrains();

    std::vector<PathWrapper> reconstructPath(std::unordered_map<Node*, PathWrapper*>& from, Node* end);
    PathCacheWrapper* getCachedPath(Node* end);
    bool findPath(Node* end, PathWrapper* destPath, char* destPathSize);
};
