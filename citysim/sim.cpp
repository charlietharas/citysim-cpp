#include <SFML/Graphics.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <time.h>
#include <condition_variable>
#include <stack>
#include <set>

#include "macros.h"
#include "line.h"
#include "train.h"
#include "node.h"
#include "citizen.h"
#include "util.h"
#include "pathCache.h"

int VALID_LINES;
int VALID_NODES;
int VALID_TRAINS;

float SIM_SPEED;
float tempSimSpeed;
const int TARGET_FPS = 60;
const std::chrono::microseconds FRAME_DURATION(1000000 / TARGET_FPS);

long long unsigned int simTick;
long long unsigned int renderTick;

long long unsigned int handledCitizens;
long long unsigned int handledPathNodes;

Line lines[MAX_LINES];
Node nodes[MAX_NODES];
Train trains[MAX_TRAINS];
Citizen citizens[MAX_CITIZENS];

// track available pointers to inactive/despawned citizens that can be reactivated with addCitizen()
// using a stack is more memory-intensive, but should be more optimal to leverage caching
std::stack<Citizen*> inactiveCitizens;
// uses a set to track citizens within the global array that are currently active
std::set<Citizen*> activeCitizens;

std::mutex trainsMutex;
std::mutex citizensMutex;
std::mutex pathsMutex;

std::atomic<bool> justDidPathfinding(false);
std::atomic<bool> shouldExit(false);

bool addCitizen(Node* startNode, Node* endNode) {
	if (inactiveCitizens.size() == 0) {
		return false;
	}
	Citizen* ptr = inactiveCitizens.top();
	if (!startNode->findPath(endNode, ptr->path, &ptr->pathSize)) {
		return false;
	}
	inactiveCitizens.pop();
	ptr->status = STATUS_SPAWNED;
	ptr->currentTrain = nullptr;
	ptr->index = 0;
	ptr->timer = 0;
	handledCitizens++;
	handledPathNodes += ptr->pathSize;
	activeCitizens.insert(ptr);
	return true;
}

void generateCitizens(int spawnAmount) {
	int spawnedCount = 0;

	// int i = lastCitizenSpawnedIndex;
	while (spawnedCount < spawnAmount) {
		int startNode = rand() % VALID_NODES;
		int endNode;
		do {
			endNode = rand() % VALID_NODES;
		} while (endNode == startNode);

		// add citizen
		if (addCitizen(&nodes[startNode], &nodes[endNode])) {
			spawnedCount++;
		}
		else {
			// std::cout << "Citizen failed to generate [" << nodes[startNode].id << " : " << nodes[endNode].id << "] " << std::endl;
		}
	}
}

int init() {

	// allocate and initialize global arrays
	for (int i = 0; i < MAX_LINES; i++) {
		lines[i] = Line();
	}

	float nodesX[MAX_NODES]; // utility arrays for Node position normalization
	float nodesY[MAX_NODES];
	for (int i = 0; i < MAX_NODES; i++) {
		nodes[i] = Node();
		nodesX[i] = 0;
		nodesY[i] = 0;
	}

	for (int i = 0; i < MAX_TRAINS; i++) {
		trains[i] = Train();
	}

	for (int i = 0; i < MAX_CITIZENS; i++) {
		citizens[i] = Citizen();
		citizens[i].status = STATUS_DESPAWNED;
		inactiveCitizens.push(&citizens[i]);
	}

	// read files
	int row = 0;
	std::string line;

	// parse [id (name), color, path] to generate Lines
	std::ifstream linesCSV("lines_stations.csv");
	if (!linesCSV.is_open()) {
		std::cerr << "Error opening lines_stations.csv" << std::endl;
		return ERROR_OPENING_FILE;
	}
	std::cout << "Successfully opened lines_stations.csv" << std::endl;

	while (std::getline(linesCSV, line)) {
		std::stringstream lineStream(line);
		std::string cell;
		lines[row].size |= STATUS_SPAWNED;
		
		int col = 0;
		while (std::getline(lineStream, cell, ',')) {
			if (col == 0) {
				// line id/name (e.g. A, A_L, F)
				std::strcpy(lines[row].id, cell.c_str());
			} else if (col == 1) {
				// color (type sf::Color)
				colorConvert(&lines[row].color, cell);
			} else {
				// update Node in path
				lines[row].path[col - 2] = &nodes[std::stoi(cell)];
			}
			col++;
		}
		row++;
	}
	VALID_LINES = row;

	// parse [id (name), x, y, ridership] to generate Node objects
	std::ifstream stationsCSV("stations_data.csv");
	if (!stationsCSV.is_open()) {
		std::cerr << "Error opening stations_data.csv" << std::endl;
		return ERROR_OPENING_FILE;
	}
	std::cout << "Successfully opened stations_data.csv" << std::endl;

	row = 0;
	while (std::getline(stationsCSV, line)) {
		std::stringstream lineStream(line);
		std::string cell;
		nodes[row].status = STATUS_SPAWNED;

		int col = 0;
		while (std::getline(lineStream, cell, ',')) {
			switch (col) {
			case 0: // numerical uid
				nodes[row].numerID = std::stoi(cell);
				break;
			case 1: // station id/name (e.g. Astor Pl, 23rd St)
				std::strcpy(nodes[row].id, cell.c_str());
				break;
			case 2: // x coordinate
				nodesX[row] = std::stof(cell);
				break;
			case 3: // y coordinate
				nodesY[row] = std::stof(cell);
				break;
			case 4: // can ignore lines value
				break;
			case 5: // ridership (daily, 2019)
				nodes[row].ridership = std::stoi(cell);
				break;
			default:
				break;
			}
			col++;
		}
		nodes[row] = nodes[row];
		row++;
	}
	VALID_NODES = row;

	// normalize Node position data to screen boundaries
	float minNodeX = nodesX[0]; float maxNodeX = nodesX[0];
	float minNodeY = nodesY[0]; float maxNodeY = nodesY[0];
	for (int i = 0; i < VALID_NODES; i++) {
		if (nodesX[i] < minNodeX) minNodeX = nodesX[i];
		if (nodesX[i] > maxNodeX) maxNodeX = nodesX[i];
		if (nodesY[i] < minNodeY) minNodeY = nodesY[i];
		if (nodesY[i] > minNodeY) maxNodeY = nodesY[i];
	}
	float minMaxDiffX = maxNodeX - minNodeX;
	float minMaxDiffY = maxNodeY - minNodeY;
	for (int i = 0; i < VALID_NODES; i++) {
		nodes[i].setPosition(Vector2f(
			WINDOW_X_OFFSET + WINDOW_SCALE * WINDOW_X_SCALE * WINDOW_X * (nodesX[i] - minNodeX) / minMaxDiffX,
			WINDOW_Y_OFFSET + WINDOW_Y - WINDOW_SCALE * WINDOW_Y_SCALE * WINDOW_Y * (nodesY[i] - minNodeY) / minMaxDiffY
		));
	}

	// add Node transfer neighbors
	// TODO segment window into grid
	WALKING_LINE = Line();
	std::strcpy(WALKING_LINE.id, WALK_LINE_ID_STR);

	int transferNeighbors = 0;
	for (int i = 0; i < VALID_NODES; i++) {
		for (int j = 0; j < VALID_NODES; j++) {
			if (i == j) continue;
			if (nodes[i].dist(&nodes[j]) <= TRANSFER_MAX_DIST) {
				struct PathWrapper neighborWrapper1 = { &nodes[i], &WALKING_LINE };
				struct PathWrapper neighborWrapper2 = { &nodes[j], &WALKING_LINE };
				float dist = nodes[i].dist(&nodes[j]);
				nodes[i].addNeighbor(&neighborWrapper2, dist);
				nodes[j].addNeighbor(&neighborWrapper1, dist);
				transferNeighbors++;
			}
		}
	}

	// various preprocessing steps, generate Train objects
	VALID_TRAINS = 0;
	int lineNeighbors = 0;
	for (int i = 0; i < VALID_LINES; i++) {
		int j = 0;
		Line* line = &lines[i];

		// update Node colors for each Line
		while (line->path[j] != NULL && line->path[j]->status) {
			if (j > 0) {
				line->dist[j - 1] = line->path[j]->dist(line->path[j - 1]) * 128;
				struct PathWrapper neighborWrapper1 = { line->path[j], line };
				struct PathWrapper neighborWrapper2 = { line->path[j - 1], line };
				float dist = line->path[j]->dist(line->path[j - 1]) * TRANSFER_PENALTY_MULTIPLIER;
				line->path[j]->addNeighbor(&neighborWrapper2, dist);
				line->path[j - 1]->addNeighbor(&neighborWrapper1, dist);
				lineNeighbors++;
			}
			line->path[j]->setFillColor(sf::Color(line->color.r, line->color.g, line->color.b));
			j++;
		}

		// generate distances between Nodes on each Line
		line->dist[j - 1] = line->dist[j-2];

		// update Line size (length)
		line->size = j;

		// generate Train objects
		for (int k = 0; k < j; k+= DEFAULT_TRAIN_STOP_SPACING) {
			// generate 2 Trains (one going backward, one forward) except if at first/last stop
			int repeat = (k == 0 || k == j - 1) ? 1 : 2;
			for (int l = 0; l < repeat; l++) {
				Train* train = &trains[VALID_TRAINS];
				train->setPosition(line->path[k]->getPosition());
				train->line = line;
				train->index = k;
				train->status = STATUS_IN_TRANSIT;
				train->statusForward = (l == 1) ? STATUS_BACK : (k == j - 1) ? STATUS_BACK : STATUS_FORWARD;
				train->setFillColor(line->color);
				VALID_TRAINS++;
			}
		}

	}

	// generate initial batch of citizens
	generateCitizens(CITIZEN_SPAWN_INIT);

	std::cout << "Loaded " << VALID_NODES << " valid nodes on " << VALID_LINES << " lines" << std::endl;
	std::cout << "Initialized " << VALID_TRAINS << " trains" << std::endl;
	std::cout << "Generated " << transferNeighbors + lineNeighbors << " neighbor pairs (" << transferNeighbors << "/" << lineNeighbors << ")" << std::endl;
	std::cout << "Generated " << CITIZEN_SPAWN_INIT << " initial citizens" << std::endl;

	// tick counters used for timing various operations
	simTick = 0;
	renderTick = 0;

	// metrics used for debugging (especially in benchmark mode)
	handledCitizens = 0;
	handledPathNodes = 0;

	return AOK;
}

void renderingThread() {
	// window
	sf::ContextSettings settings;
	settings.antialiasingLevel = 4;
	sf::RenderWindow window(sf::VideoMode(WINDOW_X, WINDOW_Y), "CitySim", sf::Style::Titlebar | sf::Style::Close, settings);

	// fps limiter
	sf::Clock clock;

	// white background
	sf::RectangleShape bg(Vector2f(WINDOW_X * 10, WINDOW_Y * 10));
	bg.setPosition(WINDOW_X * -5, WINDOW_Y * -5);
	bg.setFillColor(sf::Color(255, 255, 255, 255));

	// stats text
	sf::Text text;
	sf::Font font;
	font.loadFromFile("Arial.ttf");
	text.setFont(font);
	text.setCharacterSize(16);
	text.setFillColor(sf::Color::Black);

	// draw handlers
	sf::View view(Vector2f(WINDOW_X / 2, WINDOW_Y / 2), Vector2f(WINDOW_X, WINDOW_Y));
	Vector2f panOffset(0, 0);
	float simZoom = 1.0f;
	bool drawNodes = true;
	bool drawLines = true;
	bool drawTrains = true;
	bool drawCitizens = false;
	bool paused = false;

	// generate vertex buffer for line (path) shapes
	// copy line data to 1 dimensional vertex vector
	std::vector<sf::Vertex> lineVertices;
	lineVertices.reserve(VALID_NODES);
	for (int i = 0; i < VALID_LINES; i++) {
		int j = 0;
		while (lines[i].path[j] != nullptr && lines[i].path[j]->status) {
			sf::Vector2f position = lines[i].path[j]->getPosition();
			sf::Color color = lines[i].path[j]->getFillColor();

			if (j != 0) lineVertices.push_back(sf::Vertex(position, color));
			lineVertices.push_back(sf::Vertex(position, color));
			j++;
		}
		if (!lineVertices.empty()) lineVertices.pop_back();
	}

	// copy vector data to buffer and clear leftovers
	sf::VertexBuffer linesVertexBuffer(sf::Lines, sf::VertexBuffer::Usage::Static);
	linesVertexBuffer.create(lineVertices.size());
	linesVertexBuffer.update(lineVertices.data());
	lineVertices.clear();
	lineVertices.shrink_to_fit();

	// initialize vertex array for nodes, trains
	// not sure if I should be using a VertexBuffer for these
	sf::VertexArray nodeVertices(sf::Triangles);
	sf::VertexArray trainVertices(sf::Triangles);

	float TRAIN_CAPACITY_FLOAT = float(TRAIN_CAPACITY);
	float NODE_CAPACITY_FLOAT = float(NODE_CAPACITY);

	while (window.isOpen() && !shouldExit) {
		renderTick++;

		// fps limiter
		sf::Time frameStart = clock.getElapsedTime();

		// handle window events (including pan/zoom)
		sf::Event event;
		while (window.pollEvent(event))
		{
			// close
			if (event.type == sf::Event::Closed) {
				shouldExit = true;
				window.close();
			}
			// zoom
			else if (event.type == sf::Event::MouseWheelScrolled) {
				float zoom = 1.0f + event.mouseWheelScroll.delta * ZOOM_SCALE * -1;
				simZoom *= zoom;
				if (simZoom > ZOOM_MIN && simZoom < ZOOM_MAX) {
					view.zoom(zoom);
				}
				else {
					simZoom = std::max(ZOOM_MIN, std::min(simZoom, ZOOM_MAX));
				}
			}
			// pan
			else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
				panOffset = Vector2f(sf::Mouse::getPosition(window)) - view.getCenter();
			}
			else if (event.type == sf::Event::MouseMoved && sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
				view.setCenter(Vector2f(sf::Mouse::getPosition(window)) - panOffset);
			}
			// TODO pan with arrow keys
			else if (event.type == sf::Event::KeyPressed) {
				// press 1 to toggle nodes visibility
				if (event.key.code == sf::Keyboard::Num1) {
					drawNodes = !drawNodes;
				}
				// press 2 to toggle lines visibility
				if (event.key.code == sf::Keyboard::Num2) {
					drawLines = !drawLines;
				}
				// press 3 to toggle trains visibility
				if (event.key.code == sf::Keyboard::Num3) {
					drawTrains = !drawTrains;
				}
				// press 4 to toggle citizens visibility
				if (event.key.code == sf::Keyboard::Num4) {
					drawCitizens = !drawCitizens;
				}
				// press - to decrease simulation speed (up to minimum)
				if (event.key.code == sf::Keyboard::Subtract && !paused) {
					SIM_SPEED = std::min(std::max(SIM_SPEED - SIM_SPEED_INCR, MIN_SIM_SPEED), MAX_SIM_SPEED);
				}
				// press + to increase simulation speed (up to maximum)
				if (event.key.code == sf::Keyboard::Add && !paused) {
					SIM_SPEED = std::min(std::max(SIM_SPEED + SIM_SPEED_INCR, MIN_SIM_SPEED), MAX_SIM_SPEED);
				}
				// press p or SPACE to toggle simulation pause
				// TODO pause thread instead of shutting sim speed
				if (event.key.code == sf::Keyboard::P || event.key.code == sf::Keyboard::Space) {
					paused = !paused;
					if (paused) {
						tempSimSpeed = SIM_SPEED;
						SIM_SPEED = 0;
					}
					else {
						SIM_SPEED = tempSimSpeed;
					}
				}
			}
		}

		window.clear();

		window.setView(view);
		window.draw(bg);

		if (renderTick % TEXT_REFRESH_RATE == 0) {
			text.setString(std::to_string(activeCitizens.size()));
		}
		window.draw(text);

		// TODO FIXME for whatever godforsaken reason, this shit does not work with [] syntax and must use .append() and .clear()
		if (drawTrains) {
			std::lock_guard<std::mutex> trainLock(trainsMutex);
			trainVertices.clear();
			for (int i = 0; i < VALID_TRAINS; i++) {
				float newRadius = TRAIN_MIN_SIZE + trains[i].capacity / TRAIN_CAPACITY_FLOAT * (TRAIN_SIZE_DIFF);
				trains[i].updateRadius(newRadius);
				sf::Vector2f trainPositionNormalized = trains[i].getPosition() - sf::Vector2f(newRadius, newRadius);
				sf::Vector2f trainPosition = trains[i].getPosition();
				sf::Color trainColor = trains[i].getFillColor();
				for (int j = 0; j < TRAIN_N_POINTS; j++) {
					trainVertices.append(sf::Vertex(trains[i].getPoint(j) + trainPositionNormalized, trainColor));
					trainVertices.append(sf::Vertex(trainPosition, trainColor));
					trainVertices.append(sf::Vertex(trains[i].getPoint((j + 1) % TRAIN_N_POINTS) + trainPositionNormalized, trainColor));
				}
			}

			window.draw(trainVertices);

		}

		if (drawNodes) {
			nodeVertices.clear();
			for (int i = 0; i < VALID_NODES; i++) {
				float newRadius = NODE_MIN_SIZE + trains[i].capacity / NODE_CAPACITY_FLOAT * (NODE_SIZE_DIFF);
				nodes[i].updateRadius(newRadius);
				sf::Vector2f nodePosition = nodes[i].getPosition();
				sf::Vector2f nodePositionNormalized = nodePosition - sf::Vector2f(newRadius, newRadius);
				sf::Color nodeColor = nodes[i].getFillColor();
				for (int j = 0; j < NODE_N_POINTS; j++) {
					nodeVertices.append(sf::Vertex(nodes[i].getPoint(j) + nodePositionNormalized, nodeColor));
					nodeVertices.append(sf::Vertex(nodePosition, nodeColor));
					nodeVertices.append(sf::Vertex(nodes[i].getPoint((j + 1) % NODE_N_POINTS) + nodePositionNormalized, nodeColor));
				}
			}

			window.draw(nodeVertices);

		}

		if (drawLines) {
			window.draw(linesVertexBuffer);
		}

		// TODO optimize citizen rendering
		if (drawCitizens) {
			std::lock_guard<std::mutex> citizenLock(citizensMutex);
			for (Citizen* c : activeCitizens) {
				window.draw(*c);
			}
		}

		window.display();

		// frame rate control
		sf::Time frameTime = clock.getElapsedTime() - frameStart;
		sf::Int64 sleepTime = FRAME_DURATION.count() - frameTime.asMicroseconds();
		if (sleepTime > 0) {
			sf::sleep(sf::microseconds(sleepTime));
		}
	}
}

void pathfindingThread() {
	while (!shouldExit) {
		// TODO poke thread instead of using boolean limiter
		if (simTick % CITIZEN_SPAWN_FREQ == 0 && !justDidPathfinding) {
			justDidPathfinding = true;
			std::lock_guard<std::mutex> lock(citizensMutex);
			int spawnAmount = (CITIZEN_RANDOMIZE_SPAWN_AMT) ? rand() % (CITIZEN_SPAWN_MAX) : CITIZEN_SPAWN_MAX;
			generateCitizens(spawnAmount);
		}
	}
}

void simulationThread() {
	SIM_SPEED = DEFAULT_SIM_SPEED;
	double timeElapsed = double(clock());

	std::vector<size_t> activeCitizensStat;
	activeCitizensStat.reserve(BENCHMARK_TICK_DURATION / BENCHMARK_STAT_RATE);

	while (!shouldExit) {

		simTick++;

		#if BENCHMARK_MODE == 1
		if (simTick % BENCHMARK_STAT_RATE == 0) {
			std::cout << "\rProgress: " << float(simTick) / BENCHMARK_TICK_DURATION * 100 << "%" << ", " << activeCitizens.size() << " active citizens" << std::flush;
		}
		if (simTick >= BENCHMARK_TICK_DURATION) {
			std::cout << std::endl << "Benchmark concluded at tick " << simTick << std::endl;
			std::cout << "Handled " << handledCitizens << " citizens with " << handledPathNodes << " handled path nodes" << std::endl;
			shouldExit = true;
		}
		#endif

		// stats are recorded even outside of benchmark mode
		if (simTick % BENCHMARK_STAT_RATE == 0) {
			activeCitizensStat.push_back(activeCitizens.size());
		}

		justDidPathfinding = false;

		{
			std::lock_guard<std::mutex> lock(trainsMutex);
			float trainSpeed = TRAIN_SPEED * SIM_SPEED;
			for (int i = 0; i < VALID_TRAINS; i++) {
				trains[i].updatePositionAlongLine(trainSpeed);
			}
		}

		{
			std::lock_guard<std::mutex> lock(citizensMutex);
			float citizenSpeed = CITIZEN_SPEED * SIM_SPEED;
			for (Citizen* c : activeCitizens) {
				if (c->status != STATUS_DESPAWNED) {
					// add all newly despawned citizens to the list of available indices
					if (c->updatePositionAlongPath(citizenSpeed)) {
						activeCitizens.erase(activeCitizens.find(c));
					}
				}
			}
		}
	}
	std::cout << "Simulation time elapsed: " << (double(clock()) - timeElapsed) / CLOCKS_PER_SEC << "s" << std::endl;
	size_t averageActiveCitizens = 0; for (size_t i : activeCitizensStat) averageActiveCitizens += i;
	averageActiveCitizens /= activeCitizensStat.size();
	std::cout << "Averaged " << averageActiveCitizens << " citizen agents" << std::endl;
}

int main()
{
	// initialize simulation global arrays
	int initStatus = init();
	double progStartTime = double(clock());
	if (initStatus == AOK) {
		std::cout << "Simulation initialized successfully (" << (double(clock())-progStartTime) / CLOCKS_PER_SEC << "s)" << std::endl;
	} else {
		return initStatus;
	}

	std::thread renThread;
	if (BENCHMARK_MODE) {
		std::cout << "Simulation running in benchmark mode (" << BENCHMARK_TICK_DURATION << " ticks)" << std::endl;
		std::cout << "Citizens spawn " << CITIZEN_SPAWN_MAX << "/" << CITIZEN_SPAWN_FREQ << " ticks, max " << MAX_CITIZENS << std::endl;
	}
	else {
		renThread = std::thread(renderingThread);
	}

	std::thread simThread(simulationThread);
	std::thread pathThread(pathfindingThread);

	if (!BENCHMARK_MODE && renThread.joinable()) {
		renThread.join();
	}
	if (simThread.joinable()) {
		simThread.join();
	}
	if (pathThread.joinable()) {
		pathThread.join();
	}

	std::cout << std::endl << "Done!" << std::endl;

	return 0;
}
