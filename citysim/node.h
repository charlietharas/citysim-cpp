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
    unsigned short int level;
    unsigned long int totalRiders;
    PathWrapper neighbors[NODE_N_NEIGHBORS];
    float weights[NODE_N_NEIGHBORS];
    Train* trains[NODE_N_TRAINS];

    Node();

    bool addTrain(Train* train);
    bool removeTrain(Train* train);
    bool addNeighbor(PathWrapper& neighbor, float weight);
    bool removeNeighbor(PathWrapper& neighbor);

    inline void setGridPos(char x, char y) {
        gridPos = x << 8 | y;
    }
    inline char gridX() {
        return gridPos >> 8;
    }
    inline char gridY() {
        return gridPos & 0x0F;
    }
    inline char lowerGridX() {
        char x = gridX();
        return x > 0 ? x - 1 : x;
    }
    inline char upperGridX() {
        char x = gridX();
        return x < NODE_GRID_ROWS - 1 ? x + 1 : x;
    }
    inline char lowerGridY() {
        char y = gridY();
        return y > 0 ? y - 1 : y;
    }
    inline char upperGridY() {
        char y = gridY();
        return y < NODE_GRID_COLS - 1 ? y + 1 : y;
    }
    unsigned int numTrains();

    static std::vector<PathWrapper> bidirectionalAStar(Node* start, Node* end);
    bool findPath(Node* end, PathWrapper* destPath, char* destPathSize);
};
