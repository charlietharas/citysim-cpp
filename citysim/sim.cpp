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
#include <future>
#include <random>

#include "macros.h"
#include "util.h"
#include "line.h"
#include "node.h"
#include "pcw.h"
#include "train.h"
#include "citizen.h"

int VALID_LINES;
int VALID_NODES;
int VALID_TRAINS;

float SIM_SPEED;
const int TARGET_FPS = 60;
const std::chrono::microseconds FRAME_DURATION(1000000 / TARGET_FPS);

const int TARGET_SIM_FPS = 1000;
const std::chrono::microseconds SIM_TICK_DURATION(1000000 / TARGET_SIM_FPS);

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int> dis;

bool simRunning;
long unsigned int simTick;
long unsigned int renderTick;

unsigned int totalRidership;

int toggleSpawn;

unsigned int handledCitizens;
std::vector<size_t> activeCitizensStat;
std::vector<double> clockStat;
std::vector<double> simSpeedStat;

int NODE_GRID_ROW_SIZE;
int NODE_GRID_COL_SIZE;
std::vector<std::vector<std::vector<Node*>>> nodeGrid;

Line lines[MAX_LINES];
Node nodes[MAX_NODES];
Train trains[MAX_TRAINS];
CitizenVector citizens(MAX_CITIZENS / 2, MAX_CITIZENS);

Node* closestNode;

Line WALKING_LINE;

std::mutex trainsMutex;
std::mutex citizensMutex;
std::mutex pathsMutex;
std::mutex presetCitizenMutex;

std::atomic<bool> customSpawnCitizens(false);
std::atomic<bool> justDidPathfinding(false);
std::atomic<bool> shouldExit(false);

std::condition_variable doPathfinding;
std::condition_variable doCustomCitizenSpawn;
std::condition_variable doSimulation;

// TODO "best-practice" everything into proper .h and .cpp files, clean up comments, clean up code + formatting
// TODO use references instead of pointers for many functions

// TODO optimize for speed (likely in A*?)
void generateCitizens(int spawnAmount) {
	int spawnedCount = 0;

	// int i = lastCitizenSpawnedIndex;
	while (spawnedCount < spawnAmount) {
		int startRidership = dis(gen);
		int endRidership;
		int startNode, startRidershipCount;
		int endNode, endRidershipCount;
		int i;
		do {
			i = 0;
			startNode = 0; startRidershipCount = 0;
			endNode = 0; endRidershipCount = 0;
			endRidership = dis(gen);
			while (startRidershipCount < startRidership || endRidershipCount < endRidership) {
				if (startRidershipCount < startRidership) {
					startRidershipCount += nodes[i].ridership;
					startNode = i;
				}
				if (endRidershipCount < endRidership) {
					endRidershipCount += nodes[i].ridership;
					endNode = i;
				}
				i++;
			}
		} while (endNode == startNode);

		// add citizen
		if (citizens.add(&nodes[startNode], &nodes[endNode])) {
			handledCitizens++;
			spawnedCount++;
		}
		else {
			std::cout << "ERR: failed to find path between " << nodes[startNode].id << ", " << nodes[endNode].id << std::endl;
		}
	}
}

void debugReport() {
	std::cout << "Report at tick " << simTick << ":" << std::endl;

	std::map<std::string, int> statusMap{ {"DSPN", 0}, {"SPWN", 0}, {"MOVE", 0}, {"TSFR", 0}, {"STOP", 0}, {"WALK", 0} };
	for (int i = 0; i < citizens.size(); i++) {
		Citizen& c = citizens[i];
		switch (c.status) {
		case STATUS_DESPAWNED:
			statusMap["DSPN"]++;
			break;
		case STATUS_SPAWNED:
			statusMap["SPWN"]++;
			break;
		case STATUS_IN_TRANSIT:
			statusMap["MOVE"]++;
			break;
		case STATUS_TRANSFER:
			statusMap["TSFR"]++;
			break;
		case STATUS_AT_STOP:
			statusMap["STOP"]++;
			break;
		case STATUS_WALK:
			statusMap["WALK"]++;
			break;
		}
	}
	for (auto const& x : statusMap) {
		char buffer[8];
		std::sprintf(buffer, "%.2f", (x.second / (float) citizens.size() * 100));
		std::cout << x.first << ": " << x.second << "(" << buffer << "%)\t";
	}
	std::cout << std::endl;

	std::map<std::string, unsigned int> stuckMap;
	for (int i = 0; i < citizens.size(); i++) {
		Citizen& c = citizens[i];
		if (c.status != STATUS_DESPAWNED && c.timer > CITIZEN_DESPAWN_WARN) {
			stuckMap[c.currentPathStr()]++;
		}
	}

	if (!stuckMap.empty()) {
		std::cout << "Citizens getting stuck at: " << std::endl;
		for (auto const& x : stuckMap) {
			if (x.second >> CITIZEN_STUCK_THRESH) {
				std::cout << x.first << " ( " << x.second << ")\t";
			}
		}
		std::cout << std::endl;
	}
}

int init() {
	// utility vectors for Node position normalization
	float* nodesX = new float[MAX_NODES];
	float* nodesY = new float[MAX_NODES];

	// read files
	int row = 0;
	std::string fileLine;

	// parse [id (name), color, path] to generate Lines
	std::ifstream linesCSV("lines_stations.csv");
	if (!linesCSV.is_open()) {
		std::cerr << "Error opening lines_stations.csv" << std::endl;
		return ERROR_OPENING_FILE;
	}

	std::cout << "Reading lines_stations.csv" << std::endl;

	while (std::getline(linesCSV, fileLine)) {
		std::stringstream lineStream(fileLine);
		std::string cell;
		
		int col = 0;
		while (std::getline(lineStream, cell, ',')) {
			Line& line = lines[row];

			if (col == 0) {
				// line id/name (e.g. A, A_L, F)
				std::strcpy(line.id, cell.c_str());
			} else if (col == 1) {
				// color (type sf::Color)
				colorConvert(&line.color, cell);
			} else {
				// add Node ref to path (before Node initialization)
				line.path[col - 2] = &nodes[std::stoi(cell)];
			}
			col++;
		}
		row++;
	}

	VALID_LINES = row;
	std::cout << "Processed " << VALID_LINES << " lines" << std::endl;

	// parse [id (name), x, y, ridership] to generate Node objects
	std::ifstream stationsCSV("stations_data.csv");
	if (!stationsCSV.is_open()) {
		std::cerr << "Error opening stations_data.csv" << std::endl;
		return ERROR_OPENING_FILE;
	}

	std::cout << "Reading stations_data.csv" << std::endl;

	row = 0;
	totalRidership = 0;
	while (std::getline(stationsCSV, fileLine)) {
		std::stringstream lineStream(fileLine);
		std::string cell;
		Node& node = nodes[row];
		node.status = STATUS_EXISTS;

		int col = 0;
		while (std::getline(lineStream, cell, ',')) {
			switch (col) {
			case 0: // numerical uid
				node.numerID = std::stoi(cell);
				break;
			case 1: // station id/name (e.g. Astor Pl, 23rd St)
				std::strcpy(node.id, cell.c_str());
				break;
			case 2: // x coordinate
				nodesX[row] = (std::stof(cell));
				break;
			case 3: // y coordinate
				nodesY[row] = (std::stof(cell));
				break;
			case 4: // can ignore lines value
				break;
			case 5: // ridership (daily, 2019)
				node.ridership = std::stoi(cell);
				totalRidership += node.ridership;
				break;
			default:
				break;
			}
			col++;
		}
		row++;
	}

	VALID_NODES = row;
	std::cout << "Parsed " << VALID_NODES << " nodes (stations)" << std::endl;
	std::cout << "Total system ridership: " << totalRidership << std::endl;

	// update randint generator for citizen generation
	dis.param(std::uniform_int_distribution<int>::param_type(0, totalRidership));

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
			WINDOW_X_OFFSET + WINDOW_SCALE * WINDOW_X_SCALE * WINDOW_WIDTH * (nodesX[i] - minNodeX) / minMaxDiffX,
			WINDOW_Y_OFFSET + WINDOW_HEIGHT - WINDOW_SCALE * WINDOW_Y_SCALE * WINDOW_HEIGHT * (nodesY[i] - minNodeY) / minMaxDiffY
		));
	}

	std::cout << "Normalized node positions" << std::endl;

	// place Nodes onto grid to normalize nearestNode calculations
	NODE_GRID_ROW_SIZE = WINDOW_WIDTH / NODE_GRID_ROWS;
	NODE_GRID_COL_SIZE = WINDOW_HEIGHT / NODE_GRID_COLS;
	for (int i = 0; i < NODE_GRID_ROWS; i++) {
		std::vector<std::vector<Node*>> row;
		for (int j = 0; j < NODE_GRID_COLS; j++) {
			std::vector<Node*> cell;
			for (int k = 0; k < VALID_NODES; k++) {
				Vector2f p = nodes[k].getPosition();
				if (p.x >= i * NODE_GRID_ROW_SIZE && p.x < (i + 1) * NODE_GRID_ROW_SIZE && p.y >= j * NODE_GRID_COL_SIZE && p.y < (j + 1) * NODE_GRID_COL_SIZE) {
					cell.push_back(&nodes[k]);
					nodes[k].setGridPos(i, j);
				}
			}
			cell.shrink_to_fit();
			row.push_back(cell);
		}
		row.shrink_to_fit();
		nodeGrid.push_back(row);
	}
	nodeGrid.shrink_to_fit();

	std::cout << "Generated node grid" << std::endl;

	// add Node transfer neighbors
	WALKING_LINE = Line();
	std::strcpy(WALKING_LINE.id, WALK_LINE_ID_STR);

	int transferNeighbors = 0;
	for (int n = 0; n < VALID_NODES; n++) {
		Node& node = nodes[n];
		for (int i = node.lowerGridX(); i <= node.upperGridX(); i++) {
			for (int j = node.lowerGridY(); j <= node.upperGridY(); j++) {
				for (Node* other : nodeGrid[i][j]) {
					float dist = node.dist(other);
					if (dist < TRANSFER_MAX_DIST) {
						PathWrapper one = { &node, &WALKING_LINE };
						PathWrapper two = { other, &WALKING_LINE };
						node.addNeighbor(two, dist * TRANSFER_PENALTY_MULTIPLIER);
						other->addNeighbor(one, dist * TRANSFER_PENALTY_MULTIPLIER);
						transferNeighbors++;
					}
				}
			}
		}
	}

	std::cout << "Generated " << transferNeighbors << " walking transfer neighbors" << std::endl;

	// various preprocessing steps, generate Train objects
	int lineNeighbors = 0;

	for (int i = 0; i < VALID_LINES; i++) {
		int j = 0;
		Line& line = lines[i];

		// update Node colors for each Line
		while (j < LINE_PATH_SIZE && line.path[j] != nullptr && line.path[j]->status == STATUS_EXISTS) {
			if (j > 0) {
				line.dist[j - 1] = line.path[j]->dist(line.path[j - 1]) * 128;
				struct PathWrapper neighborWrapper1 = { line.path[j], &line };
				struct PathWrapper neighborWrapper2 = { line.path[j - 1], &line };
				float dist = line.path[j]->dist(line.path[j - 1]) * TRANSFER_PENALTY_MULTIPLIER;
				line.path[j]->addNeighbor(neighborWrapper2, dist);
				line.path[j - 1]->addNeighbor(neighborWrapper1, dist);
				lineNeighbors++;
			}
			line.path[j]->setFillColor(line.color);
			j++;
		}

		// generate distances between Nodes on each Line
		line.dist[j - 1] = line.dist[j-2];

		// update Line size (length)
		line.size = j;

		// generate Train objects
		for (int k = 0; k < j; k+= DEFAULT_TRAIN_STOP_SPACING) {
			// generate 2 Trains (one going backward, one forward) except if at first/last stop
			int repeat = (k == 0 || k == j - 1) ? 1 : 2;
			for (int l = 0; l < repeat; l++) {
				Train& train = trains[VALID_TRAINS++];
				train.setPosition(line.path[k]->getPosition());
				train.line = &line;
				train.index = k;
				train.status = STATUS_IN_TRANSIT;
				train.statusForward = (l == 1) ? STATUS_BACK : (k == j - 1) ? STATUS_BACK : STATUS_FORWARD;
				train.setFillColor(line.color);
			}
		}
	}

	std::cout << "Processed remaining node data" << std::endl;
	std::cout << "Generated " << lineNeighbors << " neighbor pairs (total " << transferNeighbors + lineNeighbors << ", " << transferNeighbors << " walk/" << lineNeighbors << " line)" << std::endl;
	std::cout << "Generated " << VALID_TRAINS << " trains" << std::endl;

	// enable continuous citizen spawning by default
	toggleSpawn = 1;

	// generate initial batch of citizens
	generateCitizens(CITIZEN_SPAWN_INIT);
	std::cout << "Generated " << CITIZEN_SPAWN_INIT << " initial citizens" << std::endl;

	delete[] nodesX;
	delete[] nodesY;

	std::cout << "INIT DONE!" << std::endl << std::endl;
	return AOK;
}

void renderingThread() {
	// window
	sf::ContextSettings settings;
	settings.antialiasingLevel = 4;
	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "CitySim C Edition", sf::Style::Titlebar | sf::Style::Close, settings);

	// fps limiter
	sf::Clock clock;

	// white background
	sf::RectangleShape bg(Vector2f(WINDOW_WIDTH * ZOOM_MIN * 2, WINDOW_HEIGHT * ZOOM_MIN * 2));
	bg.setPosition(WINDOW_WIDTH * -ZOOM_MIN, WINDOW_HEIGHT * -ZOOM_MIN);
	bg.setFillColor(sf::Color(255, 255, 255, 255));

	// stats text
	sf::Text text;
	sf::Font font;
	font.loadFromFile("Arial.ttf");
	text.setFont(font);
	text.setCharacterSize(TEXT_FONT_SIZE);
	text.setFillColor(sf::Color::Black);

	// draw handlers
	sf::View view(Vector2f(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
	Vector2f panOffset(0, 0);
	Vector2f panVelocity(0, 0);
	float simZoom = 1.0f;
	bool drawNodes = true;
	bool drawLines = true;
	bool drawTrains = true;

	// generate vertex buffer for line (path) shapes
	// copy line data to 1 dimensional vertex vector
	std::vector<sf::Vertex> lineVertices;
	lineVertices.reserve(VALID_NODES);
	for (int i = 0; i < VALID_LINES; i++) {
		int j = 0;
		while (lines[i].path[j] != nullptr && lines[i].path[j]->status == STATUS_EXISTS) {
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

	simSpeedStat.push_back(0);

	// user path stuff
	Node* node1 = nullptr;
	Node* node2 = nullptr;
	char nodeCount = 0;
	PathWrapper userPath[CITIZEN_PATH_SIZE];
	char userPathSize;
	sf::VertexBuffer userPathVertexBuffer(sf::LinesStrip, sf::VertexBuffer::Usage::Static);
	sf::Color firstColor;
	sf::Color secondColor;

	while (window.isOpen() && !shouldExit) {
		renderTick++;

		// fps limiter
		sf::Time frameStart = clock.getElapsedTime();

		// get nearest node
		float minDist = WINDOW_WIDTH + WINDOW_HEIGHT;
		closestNode = &nodes[0];
		Vector2f relMousePos = Vector2f(sf::Mouse::getPosition(window)) + view.getCenter() - Vector2f(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
		relMousePos = view.getCenter() + (relMousePos - view.getCenter()) * simZoom;
		int mx = relMousePos.x / NODE_GRID_ROW_SIZE; int my = relMousePos.y / NODE_GRID_COL_SIZE;
		mx = std::max(std::min(mx, NODE_GRID_ROWS - 1), 0); my = std::max(std::min(my, NODE_GRID_COLS - 1), 0);
		int mouseXLower = mx > 0 ? mx - 1 : mx; int mouseXUpper = mx < NODE_GRID_ROWS - 1 ? mx + 1 : mx;
		int mouseYLower = my > 0 ? my - 1 : my; int mouseYUpper = my < NODE_GRID_COLS - 1 ? my + 1 : my;
		for (int i = mouseXLower; i <= mouseXUpper; i++) {
			for (int j = mouseYLower; j <= mouseYUpper; j++) {
				for (Node* node : nodeGrid[i][j]) {
					float dist = node->dist(relMousePos.x, relMousePos.y);
					if (dist < minDist) {
						minDist = dist;
						closestNode = node;
					}
				}
			}
		}

		// handle window events (including pan/zoom)
		sf::Event event;
		while (window.pollEvent(event))
		{
			// close
			if (event.type == sf::Event::Closed) {
				simRunning = true;
				doSimulation.notify_one();
				shouldExit = true;
				window.close();
			}
			// zoom
			else if (event.type == sf::Event::MouseWheelScrolled) {
				float zoom = 1.0f + event.mouseWheelScroll.delta * ZOOM_SCALE * -1;
				simZoom *= zoom;
				if (simZoom > ZOOM_MAX && simZoom < ZOOM_MIN) {
					view.zoom(zoom);
				}
				else {
					simZoom = std::max(ZOOM_MAX, std::min(simZoom, ZOOM_MIN));
				}
			}
			// pan with mouse
			else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
				panOffset = Vector2f(sf::Mouse::getPosition(window)) - view.getCenter();
			}
			else if (event.type == sf::Event::MouseMoved && sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
				view.setCenter(Vector2f(sf::Mouse::getPosition(window)) - panOffset);
			}
			// draw custom paths by right clicking to select nearest node
			else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right) {
				std::vector<sf::Vertex> userPathVertices;
				switch (nodeCount) {
				case 0:
					node1 = closestNode;
					if (node1->getFillColor() != sf::Color::Cyan) {
						firstColor = node1->getFillColor();
					}
					node1->setFillColor(sf::Color::Cyan);
					std::cout << "User selected start " << node1->id << std::endl;
					nodeCount++;
					break;
				case 1:
					node2 = closestNode;
					if (node1 != node2 && node2->findPath(node1, userPath, &userPathSize)) {
						if (node2->getFillColor() != sf::Color::Cyan) {
							secondColor = node2->getFillColor();
						}
						node2->setFillColor(sf::Color::Cyan);
						std::cout << "User selected end " << node2->id << std::endl;
						for (char i = 0; i < userPathSize; i++) {
							userPathVertices.push_back(sf::Vertex(userPath[i].node->getPosition(), userPath[i].node->getFillColor()));
						}
						userPathVertexBuffer.create(userPathSize);
						userPathVertexBuffer.update(userPathVertices.data());
						nodeCount++;
					}
					break;
				case 2:
					std::cout << "User cleared selection" << std::endl;
					node1->setFillColor(firstColor);
					node2->setFillColor(secondColor);
					userPathSize = 0;
					userPathVertices.clear();
					userPathVertexBuffer.update(userPathVertices.data());
					nodeCount = 0;
					break;
				}
			}
			else if (event.type == sf::Event::KeyPressed) {
				// press up arrow to pan up
				if (event.key.code == sf::Keyboard::Up) {
					panVelocity += Vector2f(0, -ARROW_PAN_AMT);
				}
				// press down arrow to pan down
				if (event.key.code == sf::Keyboard::Down) {
					panVelocity += Vector2f(0, ARROW_PAN_AMT);
				}
				// press left arrow to pan left
				if (event.key.code == sf::Keyboard::Left) {
					panVelocity += Vector2f(-ARROW_PAN_AMT, 0);
				}
				// press right arrow to pan right
				if (event.key.code == sf::Keyboard::Right) {
					panVelocity += Vector2f(ARROW_PAN_AMT, 0);
				}
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
				// press - to decrease simulation speed (up to minimum)
				if (event.key.code == sf::Keyboard::Subtract && !simRunning) {
					SIM_SPEED = std::min(std::max(SIM_SPEED - SIM_SPEED_INCR, MIN_SIM_SPEED), MAX_SIM_SPEED);
				}
				// press + to increase simulation speed (up to maximum)
				if (event.key.code == sf::Keyboard::Add && !simRunning) {
					SIM_SPEED = std::min(std::max(SIM_SPEED + SIM_SPEED_INCR, MIN_SIM_SPEED), MAX_SIM_SPEED);
				}
				// press p to toggle simulation pause
				if (event.key.code == sf::Keyboard::P) {
					simRunning = !simRunning;
					doSimulation.notify_one();
				}
				// press space to spawn CUSTOM_CITIZEN_SPAWN_AMT citizens at the nearest node
				if (event.key.code == sf::Keyboard::Space) {
					{
						std::unique_lock<std::mutex> lock(presetCitizenMutex);
						customSpawnCitizens = true;
					}
					doPathfinding.notify_one();
					std::unique_lock<std::mutex> lock(presetCitizenMutex);
					doCustomCitizenSpawn.wait(lock, [] {return !customSpawnCitizens;  });
				}
				// press semicolon to output citizen vector profile for debugging
				if (event.key.code == sf::Keyboard::SemiColon) {
					std::lock_guard<std::mutex> lock(citizensMutex);
					debugReport();
				}
				// press O to toggle "passive" citizen spawning
				if (event.key.code == sf::Keyboard::O) {
					if (toggleSpawn == 0) {
						toggleSpawn = 1;
					}
					else {
						toggleSpawn = 0;
					}
				}
			}
		}

		panVelocity *= PAN_DECAY;
		panVelocity.x = std::max(std::min(panVelocity.x, MAX_PAN_VEL), -MAX_PAN_VEL);
		panVelocity.y = std::max(std::min(panVelocity.y, MAX_PAN_VEL), -MAX_PAN_VEL);
		view.move(panVelocity);

		window.clear();

		window.setView(view);
		window.draw(bg);

		if (renderTick % TEXT_REFRESH_RATE == 0) {
			size_t c = citizens.activeSize();
			std::string speedString;
			if (!simRunning) {
				double s = simSpeedStat[simSpeedStat.size() - 1];
				speedString = std::to_string(s) + " ticks/sec\n";
			}
			else {
				speedString = "Simulation paused (tick " + std::to_string(simTick) + ")\n";
			}
			text.setString(std::to_string(c) + " active citizens\n" + speedString + closestNode->id);
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

				if (nodes[i].capacity > NODE_CAPACITY_WARN_THRESH) {
					 std::cout << "ERR unusually large node " << nodes[i].id << "(" << nodes[i].capacity << ")" << std::endl;
				}

				float newRadius = NODE_MIN_SIZE + std::min(NODE_CAPACITY, nodes[i].capacity) / NODE_CAPACITY_FLOAT * (NODE_SIZE_DIFF);
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
			if (nodeCount == 2) {
				window.draw(userPathVertexBuffer);
			}
			else {
				window.draw(linesVertexBuffer);
			}
		}

		window.display();

		// frame rate control
		// vsync is for losers
		sf::Time frameTime = clock.getElapsedTime() - frameStart;
		sf::Int64 sleepTime = FRAME_DURATION.count() - frameTime.asMicroseconds();
		if (sleepTime > 0) {
			sf::sleep(sf::microseconds(sleepTime));
		}
	}
}

void pathfindingThread() {
	std::unique_lock<std::mutex> lock(pathsMutex);
	while (!shouldExit) {
		doPathfinding.wait(lock, [] {return !justDidPathfinding || customSpawnCitizens || shouldExit; });
		if (shouldExit) break;

		if (customSpawnCitizens) {
			int spawned = 0;
			for (int i = 0; i < CUSTOM_CITIZEN_SPAWN_AMT; i++) {
				Node* end = &nodes[rand() % VALID_NODES];
				if (closestNode != end && citizens.add(closestNode, end)) {
					handledCitizens++;
					spawned++;
				}
			}
			std::cout << "User spawned [" << CUSTOM_CITIZEN_SPAWN_AMT << "] at " << closestNode->id << std::endl;
			customSpawnCitizens = false;
			doCustomCitizenSpawn.notify_one();
		}
		else {
			justDidPathfinding = true;
			int spawnAmount;
			#if CITIZEN_SPAWN_METHOD == 1
			spawnAmount = CITIZEN_SPAWN_MAX;
			if (CITIZEN_RANDOMIZE_SPAWN_AMT) {
				spawnAmount = rand() % spawnAmount;
			}
			#else
			spawnAmount = TARGET_CITIZEN_COUNT - (int)citizens.activeSize();
			#endif
			generateCitizens(spawnAmount * toggleSpawn);
		}

	}
}

class CitizenThreadPool {
public:
	CitizenThreadPool(size_t numThreads) : stop(false) {
		for (size_t i = 0; i < numThreads; ++i) {
			workers.emplace_back([this] { workerThread(); });
		}
	}

	~CitizenThreadPool() {
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			stop = true;
		}
		citizenThreadCV.notify_all();
		for (std::thread& worker : workers) {
			worker.join();
		}
	}

	template<class F>
	void enqueue(F&& f) {
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			tasks.emplace(std::forward<F>(f));
		}
		citizenThreadCV.notify_one();
	}

	void waitForCompletion() {
		std::unique_lock<std::mutex> lock(queueMutex);
		citizenThreadDoneCV.wait(lock, [this] { return tasks.empty() && activeThreads == 0; });
	}

private:
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;
	std::mutex queueMutex;
	std::condition_variable citizenThreadCV;
	std::condition_variable citizenThreadDoneCV;
	std::atomic<bool> stop;
	std::atomic<int> activeThreads{ 0 };

	void workerThread() {
		while (true) {
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(queueMutex);
				citizenThreadCV.wait(lock, [this] { return stop || !tasks.empty(); });
				if (stop && tasks.empty()) {
					return;
				}
				task = std::move(tasks.front());
				tasks.pop();
			}
			activeThreads++;
			task();
			activeThreads--;
			if (tasks.empty() && activeThreads == 0) {
				citizenThreadDoneCV.notify_one();
			}
		}
	}
};

void simulationThread() {
	SIM_SPEED = DEFAULT_SIM_SPEED;
	simRunning = false;

	double timeElapsed = double(clock());
	activeCitizensStat.reserve(BENCHMARK_RESERVE);
	clockStat.reserve(BENCHMARK_RESERVE);
	simSpeedStat.reserve(BENCHMARK_RESERVE);

	CitizenThreadPool pool(NUM_THREADS);
	
	std::mutex simMutex;
	std::unique_lock<std::mutex> simLock(simMutex);
	while (!shouldExit) {
		doSimulation.wait(simLock, [] { return !simRunning; } );
		simTick++;

		#if BENCHMARK_MODE == 1
		if (simTick % BENCHMARK_STAT_RATE == 0) {
			std::cout << "\rProgress: " << float(simTick) / BENCHMARK_TICK_DURATION * 100 << "%" << ", " << citizens.activeSize() << " active citizens" << std::flush;
		}
		if (simTick >= BENCHMARK_TICK_DURATION) {
			std::cout << std::endl << "Benchmark concluded at tick " << simTick << std::endl;
			std::cout << "Handled " << handledCitizens << " citizens" << std::endl;
			shouldExit = true;
		}
		#endif

		// stats are recorded even outside of benchmark mode
		if (simTick % BENCHMARK_STAT_RATE == 0) {
			activeCitizensStat.push_back(citizens.activeSize());
			clockStat.push_back(double(clock()));
			size_t clockSize = clockStat.size();
			if (clockSize > 1) {
				simSpeedStat.push_back(BENCHMARK_STAT_RATE / ((clockStat[clockSize-1] - clockStat[clockSize-2]) / CLOCKS_PER_SEC));
			}
		}
		
		if (simTick % CITIZEN_SPAWN_FREQ == 0) {
			std::lock_guard<std::mutex> lock(pathsMutex);
			justDidPathfinding = false;
			doPathfinding.notify_one();
		}

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
			size_t chunkSize = citizens.activeSize() / NUM_THREADS + 1;

			for (int i = 0; i < NUM_THREADS; i++) {
				pool.enqueue([i, chunkSize, citizenSpeed]() {
					size_t start = i * chunkSize;
					size_t end = std::min(start + chunkSize, citizens.activeSize());
					for (size_t ind = start; ind < end; ++ind) {
						citizens.triggerCitizenUpdate(ind, citizenSpeed);
					}
					});
			}

			pool.waitForCompletion(); // this is actually hanging the main thread rn
		}
	}

	std::cout << "SIM DONE!" << std::endl;
	std::cout << "Simulation ticks elapsed: " << simTick << std::endl;
	std::cout << "Simulation time elapsed: " << (double(clock()) - timeElapsed) / CLOCKS_PER_SEC << "s" << std::endl;
	size_t averageActiveCitizens = 0; 
	for (size_t i : activeCitizensStat) averageActiveCitizens += i;
	averageActiveCitizens /= activeCitizensStat.size();
	std::cout << "Averaged " << averageActiveCitizens << " citizen agents" << std::endl;
	std::cout << "Handled total " << handledCitizens << " citizen agents" << std::endl;

	doPathfinding.notify_one();
}

int main()
{
	// initialize simulation global arrays
	double progStartTime = double(clock());
	int initStatus = init();
	if (initStatus == AOK) {
		std::cout << "Simulation initialized successfully (" << (double(clock()) - progStartTime) / CLOCKS_PER_SEC << "s)" << std::endl << std::endl;
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

	return 0;
}
