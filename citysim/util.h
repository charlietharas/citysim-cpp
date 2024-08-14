#pragma once

#include <SFML/Graphics.hpp>

// utility function to update bits [startBit, endBit) of status to value
// WARNING: does not check parameters, ensure valid preconditions
void updateStatus(unsigned int* status, char startBit, char endBit, unsigned int value) {
	char bitDiff = endBit - startBit;
	unsigned int mask = ((1 << (bitDiff)) - 1) << (sizeof(unsigned int) * 8 - endBit);
	*status &= ~mask;
	value = (value & ((1 << bitDiff) - 1)) << (32 - endBit);
	*status |= value;
}

// utility function to parse hex string into sf::Color
// requires 6 character string
void colorConvert(sf::Color* v, const std::string& a) {
	v->r = std::stoi(a.substr(0, 2), nullptr, 16);
	v->g = std::stoi(a.substr(2, 2), nullptr, 16);
	v->b = std::stoi(a.substr(4, 2), nullptr, 16);
	v->a = 255;
}