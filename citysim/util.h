#pragma once

#include <SFML/Graphics.hpp>

namespace util {
	// utility function to parse hex string into sf::Color
	// requires 6 character string
	void colorConvert(sf::Color* v, const std::string& a);

	// utility function to update capacity of node/train by -1 without uint overflow
	void subCapacity(unsigned int* ptr);
}