#pragma once

#include <vector>
#include <stack>
#include <string>
#include "handles.h"
#include "macros.h"
#include "line.h"

struct CitizenStore {
    std::vector<float> timer;
    std::vector<char> status;
    std::vector<char> index;
    std::vector<char> pathSize;
    std::vector<char> statusForward;
    std::vector<float> dist;
    std::vector<TrainHandle> currentTrain;
    std::vector<uint16_t> currentNode;
    std::vector<Line*> currentLine;
    std::vector<uint16_t> nextNode;
    std::vector<PathWrapper> path[CITIZEN_PATH_SIZE];

    std::vector<uint32_t> generation;
    std::stack<uint32_t> free_indices;

    CitizenHandle create() {
        if (free_indices.empty()) {
            uint32_t id = timer.size();
            timer.emplace_back();
            status.emplace_back();
            index.emplace_back();
            pathSize.emplace_back();
            statusForward.emplace_back();
            dist.emplace_back();
            currentTrain.emplace_back();
            currentNode.emplace_back();
            currentLine.emplace_back();
            nextNode.emplace_back();
            for (int i = 0; i < CITIZEN_PATH_SIZE; ++i) {
                path[i].emplace_back();
            }
            generation.emplace_back(1);
            return { id, generation[id] };
        } else {
            uint32_t id = free_indices.top();
            free_indices.pop();
            generation[id]++;
            return { id, generation[id] };
        }
    }

    void destroy(CitizenHandle handle) {
        if (handle.id < generation.size() && generation[handle.id] == handle.generation) {
            status[handle.id] = STATUS_DESPAWNED;
            free_indices.push(handle.id);
        }
    }
};

struct TrainStore {
    std::vector<char> status;
    std::vector<char> statusForward;
    std::vector<char> index;
    std::vector<char> nextIndex;
    std::vector<unsigned int> capacity;
    std::vector<float> timer;
    std::vector<float> dist;
    std::vector<Line*> line;
    std::vector<sf::Vector2f> position;
    std::vector<sf::Color> color;

    std::vector<std::vector<CitizenHandle>> passengers;

    std::vector<uint32_t> generation;
    std::stack<uint32_t> free_indices;

    TrainHandle create() {
        uint32_t id = status.size();
        status.emplace_back();
        statusForward.emplace_back();
        index.emplace_back();
        nextIndex.emplace_back();
        capacity.emplace_back();
        timer.emplace_back();
        dist.emplace_back();
        line.emplace_back();
        position.emplace_back();
        color.emplace_back();
        passengers.emplace_back();
        generation.emplace_back(1);
        return { id, generation[id] };
    }
};
