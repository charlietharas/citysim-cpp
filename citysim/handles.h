#pragma once

#include <cstdint>

struct CitizenHandle {
    uint32_t id;
    uint32_t generation = 0;

    bool operator==(const CitizenHandle& other) const {
        return id == other.id && generation == other.generation;
    }
};

struct TrainHandle {
    uint32_t id;
    uint32_t generation = 0;

    bool operator==(const TrainHandle& other) const {
        return id == other.id && generation == other.generation;
    }
};
