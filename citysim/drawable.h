#pragma once

#include <SFML/Graphics.hpp>
#include "macros.h"

typedef sf::Vector2f Vector2f;

class Drawable : public sf::CircleShape {
public:
	Drawable(float radius = 5.0f, int numPoints = 20, sf::Vector2f pos = sf::Vector2f(0, 0)) :
		sf::CircleShape(radius) {
		sf::CircleShape::setPosition(pos);
		sf::CircleShape::setOrigin(radius, radius);
		sf::CircleShape::setFillColor(sf::Color::Black);
		sf::CircleShape::setPointCount(numPoints);
	}

	inline void goTo(Drawable* other) {
		sf::CircleShape::setPosition(other->getPosition());
	}

	inline Vector2f lerp(float t, Drawable* other) {
		t = std::max(0.0f, std::min(t, 1.0f));
		return getPosition() * (1.0f - t) + (other->getPosition() * t);
	}

	inline float dist(Drawable* other) {
		Vector2f delta = other->getPosition() - getPosition();
		return sqrt(delta.x * delta.x + delta.y * delta.y);
	}

	inline float dist(float x, float y) {
		Vector2f delta = Vector2f(x, y) - getPosition();
		return sqrt(delta.x * delta.x + delta.y * delta.y);
	}

	void updateRadius(float radius) {
		setRadius(radius);
		setOrigin(radius, radius);
	}
};