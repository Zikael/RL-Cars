#include <iostream>

#include <string>
#include <array>
#include <thread>

#include <SFML/Graphics.hpp>

#include "TrackInfo.h"
#include "EDirection.h"
#include "Car.h"
#include "CarBatch.h"

int main()
{
	// Create the main window
	sf::RenderWindow window(sf::VideoMode(1024, 768), "RL-Cars");

	// Setup the text writer
	sf::Font font;
	if (!font.loadFromFile("./font/OPen 24 Display St.ttf"))
		return EXIT_FAILURE;
	sf::Text text;
	text.setFont(font); 
	text.setCharacterSize(32);
	text.setFillColor(sf::Color::Black);
	text.setPosition(10.f, 0.f);

	// Load the background track
	sf::Texture trackTexture;
	sf::Sprite track;
	if (!trackTexture.loadFromFile("./tracks/track1.png"))
		return EXIT_FAILURE;
	track.setTexture(trackTexture);

	// Keep an image of the track for pixel calculation
	sf::Image trackImage = trackTexture.copyToImage();

	// Load config info for the track
	TrackInfo trackInfo;
	if (trackInfo.loadTrackInfo("./tracks/track1.ini") != EXIT_SUCCESS)
		return EXIT_FAILURE;

	// Find our waypoints
	std::vector<std::vector<sf::Vertex>> waypoints = trackInfo.findWaypoints(trackImage);
	std::cout << "Generated " << waypoints.size() << " waypoints:" << std::endl;
	for (size_t idx = 0; idx < waypoints.size(); ++idx)
		std::cout << "\tWaypoint {" << std::to_string(idx) 
			<< "} A(" << waypoints[idx].at(0).position.x  << ", " << waypoints[idx].at(0).position.y 
			<< ") B(" << waypoints[idx].at(1).position.x << ", " << waypoints[idx].at(1).position.y
			<< ")" << std::endl;


	const size_t CAR_THREAD_COUNT = 3;
	const size_t CARS_PER_THREAD = 30;

	const int NETWORK_INPUT_COUNT = 5;
	const int NETWORK_OUTPUT_COUNT = 4;
	const int NETWORK_HIDDEN_COUNT = 2;
	const int NETWORK_NEURON_COUNT = 5;

	std::vector<Car> cars;
	for (size_t idx = 0; idx < CAR_THREAD_COUNT * CARS_PER_THREAD; ++idx)
	{
		Network* network = new Network(
			NETWORK_INPUT_COUNT,
			NETWORK_OUTPUT_COUNT,
			NETWORK_HIDDEN_COUNT,
			NETWORK_NEURON_COUNT
		);
		cars.push_back(Car(idx, trackInfo, trackImage, waypoints, network));
	}
		
	std::vector<std::thread> threads(CAR_THREAD_COUNT);

	std::vector<CarBatch> batches(CAR_THREAD_COUNT);
	for (size_t idx = 0; idx < CAR_THREAD_COUNT; ++idx)
	{
		std::vector<std::function<void()>> funcs(CARS_PER_THREAD);
		for (size_t inner = 0; inner < CARS_PER_THREAD; ++inner)
		{
			funcs[inner] = std::function<void()>([&cars, idx, inner, CARS_PER_THREAD] {
				cars[(CARS_PER_THREAD * idx) + inner].run();
			});
		}

		batches[idx] = CarBatch(funcs);
	}

	for (size_t idx = 0; idx < CAR_THREAD_COUNT; ++idx)
		threads[idx] = std::thread([&batches, idx] {
			batches[idx].run();
		});

	std::cout << "Created " << cars.size() << " cars" << std::endl;

	// Find the FPS of the scene
	float fps;
	sf::Clock clock = sf::Clock::Clock();
	sf::Time previousTime = clock.getElapsedTime();
	sf::Time currentTime;

	Car* bestCar = &cars[0];
	unsigned int bestScore = 0;
	unsigned int generation = 0;
	unsigned int alive = 0;

	while (window.isOpen())
	{
		// Process events
		sf::Event event;
		while (window.pollEvent(event))
		{
			// Close window: exit
			if (event.type == sf::Event::Closed)
			{
				window.close();
				
				// Kill the threads
				for (size_t idx = 0; idx < batches.size(); ++idx)
					batches[idx].stop();
				for (auto& t : threads)
					t.join();
			}
			
		}

		// Clear screen
		window.clear();
			
		// Draw the background track
		window.draw(track);

		// Check if all cars are dead
		alive = 0;

		// Draw the cars + lines
		for (std::vector<Car>::iterator car = cars.begin(); car != cars.end(); ++car)
		{
			if (car->getScore() >= bestScore)
			{
				if (car->getScore() > bestScore)
				{
					bestScore = car->getScore();
					bestCar = &(*car);
				}

				// The score is equal -> Get the faster car
				if (car->getTimeAlive() < bestCar->getTimeAlive())
				{
					bestScore = car->getScore();
					bestCar = &(*car);
				}
			}
				
			if (!car->isDead()) 
			{
				++alive;

#ifdef DRAW_LINES
				std::array<sf::Vertex[2], 5> lines = car->getLines();
				for (int idx = 0; idx < lines.size(); ++idx)
					window.draw(lines[idx], 2, sf::Lines);
#endif
			}
			window.draw(car->sprite);
		}

		if (alive == 0)
		{
			++generation;

			if (bestScore == 1)
			{
				std::cout << "All cars failed to score! Creating new weights..." << std::endl;
				for (std::vector<Car>::iterator car = cars.begin(); car != cars.end(); ++car)
				{
					car->getNetwork()->setupWeights();
					car->reset();
				}
			}
			else
			{
				std::cout << "Best performing car was: " << bestCar->getID() << " | Score: " << bestScore <<
					" (Time alive: " << bestCar->getTimeAlive() << "s)" << std::endl;
#ifdef DUMP_BEST
				bestCar->getNetwork()->dump();
#endif
				std::vector<Layer*> bestLayers = bestCar->getNetwork()->getLayers();
				for (std::vector<Car>::iterator car = cars.begin(); car != cars.end(); ++car)
				{
					if (*car != bestCar)
						car->getNetwork()->mutate(bestLayers, .5f);
					car->reset();
				}
			}
			
			// Reset best car 
			bestCar = &cars[0];
			bestScore = 0;
		}
			
		// Calculate FPS
		currentTime = clock.getElapsedTime();
		fps = 1.0f / (currentTime.asSeconds() - previousTime.asSeconds());
		previousTime = currentTime;

		text.setString(
			"FPS: " + std::to_string((int)fps) + '\n' +
			"Generation: " + std::to_string(generation) + '\n' +
			"Alive: " + std::to_string(alive) + "/" + std::to_string(CAR_THREAD_COUNT * CARS_PER_THREAD)
		);

		window.draw(text);
		
		// Draw scene
		window.display();
	}

	return 0;
}