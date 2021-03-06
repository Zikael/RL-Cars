#include <vector>
#include <string>
#include <filesystem>
#include <assert.h>
#include <random>
#include <stdlib.h>

#include <SFML/Graphics.hpp>

#include "Car.h"

#define PI (float)3.14159

Car::Car(unsigned int id, TrackInfo& trackInfo, sf::Image& track, std::vector<std::vector<sf::Vertex>>& waypoints, Network* network)
	: id(id), trackInfo(trackInfo), track(track), waypoints(waypoints), network(network)
{
	sprite.setPosition(trackInfo.posX, trackInfo.posY);
	sprite.setScale(trackInfo.scaleX, trackInfo.scaleY);
	sprite.setRotation(trackInfo.rotation);

	// Let's set a texture for this car
	assert(loadTexture() == EXIT_SUCCESS);

	// Set the origin to the centre for rotation
	sprite.setOrigin(sf::Vector2f(sprite.getTexture()->getSize().x * 0.5f, sprite.getTexture()->getSize().y * 0.5f));

	previousBounds = sprite.getLocalBounds();
	previousPos = sprite.getPosition();

	updatePoints();
	findLines();
}

void Car::reset()
{
	sprite.setPosition(trackInfo.posX, trackInfo.posY);
	sprite.setRotation(trackInfo.rotation);

	previousPos = sprite.getPosition();

	updateRect();
	updatePoints();
	findLines();

	// Revert Alpha
	sf::Color current = sprite.getColor();
	current.a = 255;
	sprite.setColor(current);

	laps.waypointsPassed.clear();
	laps.lap = 0;

	moveCount = 0;

	start.restart();

	// Bring the car back to life
	dead = false;
}

void Car::waypointCollision()
{	
	// Check if we have advanced a lap
	if (laps.waypointsPassed.size() == waypoints.size())
	{
		// We have passed all waypoints - check we have crossed the finish line
		advanceLap();
		return;
	}

	size_t startSize = laps.waypointsPassed.size();

	// Treat each edge of the car sprite as a line - check for line collision with it
	for (std::vector<std::vector<sf::Vertex>>::iterator waypoint = waypoints.begin(); waypoint != waypoints.end(); ++waypoint)
	{
		std::vector<std::vector<sf::Vertex>>::iterator find = std::find_if(
			laps.waypointsPassed.begin(),
			laps.waypointsPassed.end(),
			[&](std::vector<sf::Vertex> v) {
				return (waypoint->at(0).position == v.at(0).position && waypoint->at(1).position == v.at(1).position);
			}
		);

		if (find != laps.waypointsPassed.end())
			continue;

		// Top
		if (lineIntersection(waypoint->at(0).position, waypoint->at(1).position, points[0], points[2]))
		{
			laps.waypointsPassed.push_back(*waypoint);
			break;
		}
		// Left
		if (lineIntersection(waypoint->at(0).position, waypoint->at(1).position, points[0], points[3]))
		{
			laps.waypointsPassed.push_back(*waypoint);
			break;
		}
		// Right
		if (lineIntersection(waypoint->at(0).position, waypoint->at(1).position, points[2], points[5]))
		{
			laps.waypointsPassed.push_back(*waypoint);
			break;
		}
		// Bottom
		if (lineIntersection(waypoint->at(0).position, waypoint->at(1).position, points[3], points[5]))
		{
			laps.waypointsPassed.push_back(*waypoint);
			break;
		}
	}
}

unsigned int Car::getScore()
{
	return (laps.lap + 1) * (laps.waypointsPassed.size() + 1);
}

unsigned int Car::getID()
{
	return id;
}

Network* Car::getNetwork()
{
	return network;
}

bool Car::isDead()
{
	return dead;
}

float Car::getTimeAlive()
{
	return timeAlive;
}

std::array<sf::Vertex[2], 5>& Car::getLines()
{
	return lines;
}

Car::Car(const Car& c) :
	network(c.network),
	id(c.id),
	waypoints(c.waypoints),
	trackInfo(c.trackInfo),
	track(c.track),
	texture(c.texture),
	sprite(c.sprite)
{ /* Empty */ }

int Car::loadTexture()
{
	std::vector<std::string> images;
	for (auto& file : std::filesystem::directory_iterator("./images"))
		images.push_back((file.path().string()));

	std::random_device dev;
	std::uniform_int_distribution<int> dist(0, images.size() - 1);
	std::mt19937 mt(dev());

	if (!texture->loadFromFile(images[dist(mt)]))
		return EXIT_FAILURE;

	sprite.setTexture(*texture);

	return EXIT_SUCCESS;
}

inline sf::Vertex Car::findLine(TrackInfo::RGB& roadColour, sf::Image& track,
	std::function<sf::Vector2<int>(const sf::FloatRect&, const sf::Transform&, int)> update)
{
	const int MAX_MARCH = 1000;
	const int ACCURACY_THRESHOLD = MAX_MARCH / 50;

	unsigned int march = 0;
	sf::Vector2<int> point;
	do
	{
		point = update(rect, trans, march);
		if (march >= MAX_MARCH)
			break;
		// Get less accurate the larger march is
		if (march >= ACCURACY_THRESHOLD) march *= 1.5;
		else ++march;
		// Check bounds
		if (point.x < 0) point.x = 0;
		if (point.x >= track.getSize().x) point.x = track.getSize().x - 1;
		if (point.y < 0) point.y = 0;
		if (point.y >= track.getSize().y) point.y = track.getSize().y - 1;
	} while (validColour(track.getPixel(point.x, point.y)));

	return sf::Vertex({ (float)point.x, (float)point.y });
}

inline int Car::getLength(sf::Vertex& a, sf::Vertex& b)
{
	return std::sqrt(
		(b.position.x - a.position.x) * (b.position.x - a.position.x) +
		(b.position.y - a.position.y) * (b.position.y - a.position.y)
	);
}

void Car::setDead()
{
	// The car has crashed!
	// Change state to dead
	dead = true;

	// Record time alive
	timeAlive = start.getElapsedTime().asSeconds();

	sf::Color current = sprite.getColor();
	current.a = 100;
	sprite.setColor(current);
}

inline bool Car::validColour(const sf::Color& pixel)
{
	return trackInfo.roadColour == pixel || trackInfo.lineColour == pixel || trackInfo.waypointColour == pixel;
}

inline void Car::updateRect()
{
	rect = sprite.getLocalBounds();
	trans = sprite.getTransform();
}

void Car::updatePoints()
{
	// Find the points to detect collision
	// To speed this up we will only look at each vertex
	// and the mid point between each vertex pair

	// Top verticies
	points[0] = trans * sf::Vector2f({ rect.left, rect.top });
	points[1] = trans * sf::Vector2f({ (rect.left + rect.width) / 2, rect.top });
	points[2] = trans * sf::Vector2f({ rect.left + rect.width, rect.top });

	// Bottom verticies
	points[3] = trans * sf::Vector2f({ rect.left, rect.top + rect.height });
	points[4] = trans * sf::Vector2f({ (rect.left + rect.width) / 2, rect.top + rect.height });
	points[5] = trans * sf::Vector2f({ rect.left + rect.width, rect.top + rect.height });

	// Middle verticies
	points[6] = trans * sf::Vector2f({ rect.left, (rect.top + rect.height) / 2 });
	points[7] = trans * sf::Vector2f({ rect.left + rect.width, (rect.top + rect.height) / 2 });
}

void Car::checkStuck()
{
	// This function combats cars spamming LEFT & RIGHT or FORWARDS & BACKWARDS
	if (withinToleranceOfSprite(previousPos, MOVE_SPEED * 2.f))
		setDead();

	// Check the car is making progress
	if (moveCount >= 1000)
	{
		moveCount = 0;

		if (getScore() == lastScore)
			setDead();
		else
			lastScore = getScore();
	}
		
	previousPos = sprite.getPosition();
}

inline bool Car::withinTolerance(sf::Vector2f& v1, sf::Vector2f& v2, float tolerance)
{
	sf::Vector2f res = v2 - v1;
	return (abs(res.x) < tolerance && abs(res.y) < tolerance);
}

inline bool Car::withinToleranceOfSprite(sf::Vector2f& v1, float tolerance)
{
	sf::Vector2f res = sprite.getPosition() - v1;
	return (abs(res.x) < tolerance && abs(res.y) < tolerance);
}

void Car::findLines()
{

	// Check if the car has moved, no need to re-compute if not
	if (rect == previousBounds)
		return;

	lines[0][0] = sf::Vertex({ trans * sf::Vector2f({ (rect.left + rect.width) / 2.f, rect.top }) });
	lines[0][1] = findLine(
		trackInfo.roadColour,
		track,
		[](const sf::FloatRect& rect, const sf::Transform& trans, int march) -> sf::Vector2<int> {
			return (sf::Vector2<int>)(trans * sf::Vector2f((rect.left + rect.width) / 2.f, rect.top - march));
		}
	);


	lines[1][0] = sf::Vertex({ trans * sf::Vector2f({ rect.left + rect.width, rect.top }) });
	lines[1][1] = findLine(
		trackInfo.roadColour,
		track,
		[](const sf::FloatRect& rect, const sf::Transform& trans, int march) -> sf::Vector2<int> {
			return (sf::Vector2<int>)(trans * sf::Vector2f(rect.width + march / 2.f,  rect.top - march / 2.f)); 
		}
	);

	lines[2][0] = sf::Vertex({ trans * sf::Vector2f({ rect.left, rect.top }) });
	lines[2][1] = findLine(
		trackInfo.roadColour,
		track,
		[](const sf::FloatRect& rect, const sf::Transform& trans, int march) -> sf::Vector2<int> {
			return (sf::Vector2<int>)(trans * sf::Vector2f(rect.left - march / 2.f,  rect.top - march / 2.f)); 
		}
	);

	lines[3][0] = sf::Vertex({ trans * sf::Vector2f({ rect.left, (rect.top + rect.height) / 2 }) });
	lines[3][1] = findLine(
		trackInfo.roadColour,
		track,
		[](const sf::FloatRect& rect, const sf::Transform& trans, int march) -> sf::Vector2<int> {
			return (sf::Vector2<int>)(trans * sf::Vector2f(rect.left - march, (rect.left + rect.height) / 2.f));
		}
	);

	lines[4][0] = sf::Vertex({ trans * sf::Vector2f({ rect.left + rect.width, (rect.top + rect.height) / 2.f }) });
	lines[4][1] = findLine(
		trackInfo.roadColour,
		track,
		[](const sf::FloatRect& rect, const sf::Transform& trans, int march) -> sf::Vector2<int> {
			return (sf::Vector2<int>)(trans * sf::Vector2f(rect.left + rect.width + march, (rect.top + rect.height) / 2.f));
		}
	);
}

std::array<int, 5> Car::getLineLengths()
{
	return std::array<int, 5>({
			getLength(lines[0][0], lines[0][1]),
			getLength(lines[1][0], lines[1][1]),
			getLength(lines[2][0], lines[2][1]),
			getLength(lines[3][0], lines[3][1]),
			getLength(lines[4][0], lines[4][1]),
		});
}

void Car::borderCollision()
{
	// Check for colour collision on each point
	for (int idx = 0; idx < 8; ++idx)
	{
		const sf::Color pixel = track.getPixel(points[idx].x, points[idx].y);
		if (!validColour(pixel))
		{
			setDead();
			break;
		}
	}
}

// p1 & p2 are the two points that define line A
// p3 & p4 are the two points that define line B
inline bool Car::lineIntersection(sf::Vector2f& p1, sf::Vector2f& p2, sf::Vector2f& p3, sf::Vector2f& p4)
{
	const float denominator = ((p4.y - p3.y) * (p2.x - p1.x) - (p4.x - p3.x) * (p2.y - p1.y));
	const float a = ((p4.x - p3.x) * (p1.y - p3.y) - (p4.y - p3.y) * (p1.x - p3.x)) / denominator;
	const float b = ((p2.x - p1.x) * (p1.y - p3.y) - (p2.y - p1.y) * (p1.x - p3.x)) / denominator;

	return (a >= 0 && a <= 1 && b >= 0 && b <= 1);
}

void Car::advanceLap()
{
	if (laps.waypointsPassed.size() != waypoints.size())
		return;

	// Check for colour collision on each point
	for (int idx = 0; idx < 8; ++idx)
	{
		const sf::Color pixel = track.getPixel(points[idx].x, points[idx].y);
		if (trackInfo.lineColour == pixel)
		{
			// We are on the line!
			laps.waypointsPassed.clear();
			++laps.lap;

			return;
		}
	}
}



void Car::move(EDirection dir)
{
	if (dead)
		return;

	updateRect();

	if (dir == EDirection::FORWARD)
	{
		speed += MOVE_SPEED;
		if (speed > MAX_SPEED)
			speed = MAX_SPEED;
	}
	else if (dir == EDirection::BACKWARD)
	{
		speed -= MOVE_SPEED;
		if (speed < -MAX_SPEED)
			speed = -MAX_SPEED;
	}
	else if (dir == EDirection::ROTATE_LEFT)
	{
		sprite.rotate(-speed * ROTATE_SPEED_RATIO);
	}
	else if (dir == EDirection::ROTATE_RIGHT)
	{
		sprite.rotate(speed * ROTATE_SPEED_RATIO);
	}
		

	if (speed < 0.1f && speed > -0.1f)
		return;

	const float angleRADS = (PI / 180.f) * (sprite.getRotation() + 270.f);
	sprite.move({ speed * cos(angleRADS), speed * sin(angleRADS) });

	// Check we haven't hit anything
	updatePoints();

	findLines();
	borderCollision();
	waypointCollision();
}

void Car::findMove()
{
	std::array<int, 5> lines = getLineLengths();
	move(network->predictMove(lines));

	if (++moveCount % 100 == 0)
		checkStuck();
}

void Car::run()
{
	while (acc > ups)
	{
		acc -= ups;
		if (!dead)
			findMove();
	}
	acc += clock.restart();
}

