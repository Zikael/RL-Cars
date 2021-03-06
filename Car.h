#ifndef _CAR_H
#define _CAR_H

#include <array>
#include <vector>
#include <functional>

#include "Network.h"
#include "TrackInfo.h"
#include "EDirection.h"

class Car
{
private:

	std::array<sf::Vertex[2], 5> lines;
	sf::FloatRect previousBounds;

	sf::Vector2f previousPos;

	size_t moveCount = 0;
	unsigned int lastScore = 0;

	std::vector<std::vector<sf::Vertex>> waypoints;
	sf::Vector2f points[8];

	// Updated within the Move function
	sf::FloatRect rect;
	sf::Transform trans;

	unsigned int id;

	bool dead = false;

	const float MOVE_SPEED = .3f;
	const float ROTATE_SPEED_RATIO = 2.5f;
	const float MAX_SPEED = MOVE_SPEED * 4.f;
	float speed = 0.f;

	// Store a copy of the track info and image
	TrackInfo& trackInfo;
	sf::Image& track;

	Network* network;

	sf::Clock clock;
	sf::Time acc = sf::Time::Zero;
	sf::Time ups = sf::seconds(1.f / 144.f);

	sf::Clock start;
	float timeAlive;

	void setDead();

	int loadTexture();

	void advanceLap();

	void borderCollision();
	void findLines();
	inline bool lineIntersection(sf::Vector2f&, sf::Vector2f&, sf::Vector2f&, sf::Vector2f&);
	inline sf::Vertex findLine(TrackInfo::RGB&, sf::Image&, std::function<sf::Vector2<int>(const sf::FloatRect&, const sf::Transform&, int)>);
	inline int getLength(sf::Vertex&, sf::Vertex&);

	inline bool validColour(const sf::Color&);
	inline bool validColour(sf::Color&);

	inline void updateRect();
	void updatePoints();
	void checkStuck();

	void findMove();
	void waypointCollision();
	std::array<int, 5> getLineLengths();

	inline bool withinTolerance(sf::Vector2f&, sf::Vector2f&, float tolerance);
	inline bool withinToleranceOfSprite(sf::Vector2f&, float tolerance);

	void move(EDirection);

	Car() = delete;
public:
	struct Laps
	{
		std::vector<std::vector<sf::Vertex>> waypointsPassed;
		unsigned int lap;
		Laps() : lap(0) { /* Empty */ };
	} laps;

	Car(unsigned int, TrackInfo&, sf::Image&, std::vector<std::vector<sf::Vertex>>&, Network*);
	Car(const Car& c);

	sf::Texture* texture = new sf::Texture();
	sf::Sprite sprite;

	Network* getNetwork();

	void run();
	void reset();

	unsigned int getScore();
	unsigned int getID();
	bool isDead();
	float getTimeAlive();

	std::array<sf::Vertex[2], 5>& getLines();

	bool operator==(Car& rhs) { return id == rhs.id; }
	bool operator==(const Car& rhs) const { return id == rhs.id; }

	bool operator==(Car* rhs) { return *this == rhs; }
	bool operator==(const Car* rhs) const { return *this == rhs; }

	bool operator!=(Car& rhs) { return !(*this == rhs); }
	bool operator!=(const Car& rhs) const { return !(*this == rhs); }

	bool operator!=(Car* rhs) { return !(*this == *rhs); }
	bool operator!=(const Car* rhs) const { return !(*this == *rhs); }
};

#endif