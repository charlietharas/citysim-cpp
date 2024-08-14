#pragma once

#include <algorithm>
#include <list>
#include <vector>
#include <unordered_map>

class Node;
struct PathWrapper;

class NodePairWrapper {
	Node* first;
	Node* second;
public:
	NodePairWrapper(Node* f, Node* s) {
		first = f;
		second = s;
	}
	bool operator== (const NodePairWrapper& other) const {
		return first == other.first && second == other.second;
	}
	bool operator!= (const NodePairWrapper& other) const {
		return first != other.first || second != other.second;
	}
	struct HashFunction {
		std::size_t operator()(const NodePairWrapper& wrapper) const {
			return std::hash<Node*>()(wrapper.first) & std::hash<Node*>()(wrapper.second);
		}
	};
};

struct PathCacheWrapper {
	PathWrapper* path;
    size_t pathSize;
public:
	PathCacheWrapper(PathWrapper* p, size_t s) {
		path = p;
		pathSize = s;
	}
};

template<typename KeyType, typename ValueType>
class PathCache {
public:
    PathCache(size_t c) {
        capacity = c;
    }

    void put(const KeyType& key, const ValueType& value) {
        // check if key already exists
        auto it = cacheMap.find(key);
        if (it != cacheMap.end()) {
            // move to front (if exists)
            cacheList.erase(it->second);
            cacheMap.erase(it);
        }

        // move to front (if new)
        cacheList.push_front(std::make_pair(key, value));
        cacheMap[key] = cacheList.begin();

        if (cacheList.size() > capacity) {
            // remove LRU if cache above capacity
            auto last = cacheList.end();
            last--;
            cacheMap.erase(last->first);
            cacheList.pop_back();
        }
    }

    ValueType get(const KeyType& key) {
        auto it = cacheMap.find(key);
        if (it == cacheMap.end()) {
            return PathCacheWrapper(nullptr, 0);
        }

        // move to front (if accessed, exists)
        cacheList.splice(cacheList.begin(), cacheList, it->second);
        return it->second->second;
    }

    bool exists(const KeyType& key) const {
        return cacheMap.find(key) != cacheMap.end();
    }

private:
    size_t capacity;
    std::list<std::pair<KeyType, ValueType>> cacheList;
    std::unordered_map<KeyType, typename std::list<std::pair<KeyType, ValueType>>::iterator, typename KeyType::HashFunction> cacheMap;
};
