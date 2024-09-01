#include "util.h"

// utility function to parse hex string into sf::Color
// requires 6 character string
void util::colorConvert(sf::Color* v, const std::string& a) {
	v->r = std::stoi(a.substr(0, 2), nullptr, 16);
	v->g = std::stoi(a.substr(2, 2), nullptr, 16);
	v->b = std::stoi(a.substr(4, 2), nullptr, 16);
	v->a = 255;
}

// utility function to update capacity of node/train by -1 without uint overflow
void util::subCapacity(unsigned int* ptr) {
	*ptr = std::min(*ptr - 1, 0u);
}