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
#include "line.h"
#include "node.h"
#include "pathcache.h"
#include "train.h"
#include "citizen.h"
#include "util.h"

// weighted-random node selection
unsigned int totalRidership;
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int> dis;

// simulation controls
float SIM_SPEED;
bool toggleSpawn;
bool simRunning;
long unsigned int simTick;
long unsigned int renderTick;

// statistics
unsigned int handledCitizens;
double trainHandleTime;
double citizenHandleTime;
std::vector<size_t> activeCitizensStat;
std::vector<double> clockStat;
std::vector<double> simSpeedStat;

// node grid
int NODE_GRID_ROW_SIZE;
int NODE_GRID_COL_SIZE;
std::vector<std::vector<std::vector<Node*>>> nodeGrid;

// global arrays
int VALID_LINES;
int VALID_NODES;
int VALID_TRAINS;
Line lines[MAX_LINES];
Node nodes[MAX_NODES];
Train trains[MAX_TRAINS];
CitizenVector citizens(MAX_CITIZENS / 2, MAX_CITIZENS);

// multithreading managers
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

// misc
Node* nearestNode;
Line WALKING_LINE;

// TODO "best-practice" everything into proper .h and .cpp files, clean up comments, clean up code + formatting
// TODO use references instead of pointers for many functions

// TODO optimize for speed
void generateCitizens(int spawnAmount) {
	int spawnedCount = 0;

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

		if (citizens.add(&nodes[startNode], &nodes[endNode])) {
			handledCitizens++;
			spawnedCount++;
		}
		#if ERROR_MODE == true
		else {
			std::cout << "ERR: failed to find path between " << nodes[startNode].id << ", " << nodes[endNode].id << std::endl;
		}
		#endif
	}
}

void debugReport() {
	std::cout << "Report at tick " << simTick << ":" << std::endl;

	// display statuses of allocated citizens
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
		std::sprintf(buffer, "%.1f", (x.second / (float) citizens.size() * 100));
		std::cout << x.first << ": " << x.second << "(" << buffer << "%)\t";
	}
	std::cout << std::endl;

	// display problematic path steps
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

	// display problematic nodes
	bool foundLargeNode = false;
	for (int i = 0; i < VALID_NODES; i++) {
		if (nodes[i].capacity > NODE_CAPACITY_WARN_THRESH) {
			if (!foundLargeNode) std::cout << "Unusually large nodes:" << std::endl;
			foundLargeNode = true;
			std::cout << nodes[i].id << "[" << nodes[i].numerID << "] (" << nodes[i].capacity << ")" << ", ";
		}
	}

	std::cout << std::endl;
}

// utility class to manage threads used for updating citizens every simulation tick
class CitizenThreadPool {
public:
	CitizenThreadPool(size_t numThreads) {
		stop = false;
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
		while (!shouldExit) {
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


int init() {
	// utility arrays for node position normalization
	float* nodesX = new float[MAX_NODES];
	float* nodesY = new float[MAX_NODES];

	// read files
	int row = 0;
	std::string fileLine;

	// parse [id, color, {path}] to generate lines
	std::ifstream linesCSV("lines_stations.csv");
	if (!linesCSV.is_open()) {
		std::cerr << "Error opening lines_stations.csv" << std::endl;
		return ERROR_OPENING_FILE;
	}

	std::cout << "Reading lines_stations.csv" << std::endl;

	while (std::getline(linesCSV, fileLine)) {
		std::stringstream lineStream(fileLine);
		std::string cell;
		Line& line = lines[row];

		int col = 0;
		while (std::getline(lineStream, cell, ',')) {
			if (col == 0) {
				// line id (name) (e.g. 6, A_L, F)
				std::strcpy(line.id, cell.c_str());
			} else if (col == 1) {
				// color (type sf::Color)
				util::colorConvert(&line.color, cell);
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

	// parse [numerID, id, x, y, _, ridership] to generate nodes
	std::ifstream stationsCSV("stations_data.csv");
	if (!stationsCSV.is_open()) {
		std::cerr << "Error opening stations_data.csv" << std::endl;
		return ERROR_OPENING_FILE;
	}

	std::cout << "Reading stations_data.csv" << std::endl;

	row = 0;
	while (std::getline(stationsCSV, fileLine)) {
		std::stringstream lineStream(fileLine);
		std::string cell;
		Node& node = nodes[row];
		node.status = STATUS_SPAWNED;

		int col = 0;
		while (std::getline(lineStream, cell, ',')) {
			switch (col) {
			case 0: // numerical uid
				node.numerID = std::stoi(cell);
				break;
			case 1: // station id (name) (e.g. Astor Pl, 23rd St)
				std::strcpy(node.id, cell.c_str());
				break;
			case 2: // x coordinate
				nodesX[row] = (std::stof(cell));
				break;
			case 3: // y coordinate
				nodesY[row] = (std::stof(cell));
				break;
			case 4: // can ignore lines column
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

	// normalize node position data to screen boundaries
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

	// place nodes onto grid to normalize nearestNode calculations
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

	// add node walking transfer neighbors (all nodes within TRANSFER_MAX_DIST units)
	WALKING_LINE = Line();
	std::strcpy(WALKING_LINE.id, WALK_LINE_ID_STR);

	int transferNeighbors = 0;
	for (int n = 0; n < VALID_NODES; n++) {
		Node& node = nodes[n];
		// uses the node grid to optimize calculations: may result in issues with high TRANSFER_MAX_DIST and high NODE_GRID_ROWS/COLS
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

	// various preprocessing steps, generate train objects
	int lineNeighbors = 0;

	for (int i = 0; i < VALID_LINES; i++) {
		int j = 0;
		Line& line = lines[i];

		// add line neighbors (adjacent nodes along line)
		// update node colors for each line
		while (j < LINE_PATH_SIZE && line.path[j] != nullptr && line.path[j]->status == STATUS_SPAWNED) {
			if (j > 0) {
				line.dist[j - 1] = line.path[j]->dist(line.path[j - 1]) * 128;
				struct PathWrapper one = { line.path[j], &line };
				struct PathWrapper two = { line.path[j - 1], &line };
				float dist = line.path[j]->dist(line.path[j - 1]);
				line.path[j]->addNeighbor(two, dist);
				line.path[j - 1]->addNeighbor(one, dist);
				lineNeighbors++;
			}
			line.path[j]->setFillColor(line.color);
			j++;
		}

		// generate distances between nodes on each line
		line.dist[j - 1] = line.dist[j-2];

		// update line size (length)
		line.size = j;

		// generate train objects
		std::string idStr = line.id;
		// TODO custom headways, train speeds
		int spacing = idStr.find("A_") == std::string::npos ? DEFAULT_TRAIN_STOP_SPACING / 2 : DEFAULT_TRAIN_STOP_SPACING; // avoid excessive generation for the A train
		for (int k = 0; k < j; k+= DEFAULT_TRAIN_STOP_SPACING) {
			// generate 2 trains (one going backward, one forward) except if at first/last stop
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

	// enable continuous citizen spawning by default (necessary to generate initial citizen batch)
	toggleSpawn = true;

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
	settings.antialiasingLevel = ANTIALIAS_LEVEL;
	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "CitySim", sf::Style::Titlebar | sf::Style::Close, settings);
	window.setFramerateLimit(TARGET_FPS);
	window.requestFocus();

	// fps limiter
	sf::Clock clock;

	// white background
	sf::RectangleShape bg(Vector2f(WINDOW_WIDTH * ZOOM_MIN * 2, WINDOW_HEIGHT * ZOOM_MIN * 2));
	bg.setPosition(WINDOW_WIDTH * -ZOOM_MIN, WINDOW_HEIGHT * -ZOOM_MIN);
	bg.setFillColor(sf::Color::White);

	// info text (top left)
	sf::Text text;
	sf::Font font;
	font.loadFromFile("Arial.ttf");
	text.setFont(font);
	text.setCharacterSize(TEXT_FONT_SIZE);
	text.setFillColor(sf::Color::Black);

	// draw handlers and utilities
	sf::View view(Vector2f(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
	sf::View textView(view);
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
		while (lines[i].path[j] != nullptr && lines[i].path[j]->status == STATUS_SPAWNED) {
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
	nodeVertices.resize(VALID_NODES * NODE_N_POINTS * 3);
	trainVertices.resize(VALID_TRAINS * TRAIN_N_POINTS * 3);

	// used to properly render node/train sizes
	float TRAIN_CAPACITY_FLOAT = float(TRAIN_CAPACITY);
	float NODE_CAPACITY_FLOAT = float(NODE_CAPACITY);

	// handlers to draw custom user paths
	Node* userStartNode = nullptr;
	Node* userEndNode = nullptr;
	char userNodesSelected = 0;
	PathWrapper userPath[CITIZEN_PATH_SIZE];
	char userPathSize;
	sf::VertexBuffer userPathVertexBuffer(sf::LinesStrip, sf::VertexBuffer::Usage::Static);
	sf::Color firstColor;
	sf::Color secondColor;

	// prevent out of bounds access
	simSpeedStat.push_back(0);
	clockStat.push_back(1);
	clockStat.push_back(2);

	while (window.isOpen() && !shouldExit) {
		renderTick++;

		// fps limiter
		sf::Time frameStart = clock.getElapsedTime();

		// get nearest node (uses node grid)
		float minDist = WINDOW_WIDTH + WINDOW_HEIGHT;
		nearestNode = &nodes[0];
		// calculate relative mouse position (in terms of window units, scaled to zoom + pan)
		Vector2f relMousePos = Vector2f(sf::Mouse::getPosition(window)) + view.getCenter() - Vector2f(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
		relMousePos = view.getCenter() + (relMousePos - view.getCenter()) * simZoom;
		int mx = std::max(std::min((int) relMousePos.x / NODE_GRID_ROW_SIZE, NODE_GRID_ROWS - 1), 0);
		int my = std::max(std::min((int) relMousePos.y / NODE_GRID_COL_SIZE, NODE_GRID_COLS - 1), 0);
		int mouseXLower = mx > 0 ? mx - 1 : mx; int mouseXUpper = mx < NODE_GRID_ROWS - 1 ? mx + 1 : mx;
		int mouseYLower = my > 0 ? my - 1 : my; int mouseYUpper = my < NODE_GRID_COLS - 1 ? my + 1 : my;
		for (int i = mouseXLower; i <= mouseXUpper; i++) {
			for (int j = mouseYLower; j <= mouseYUpper; j++) {
				for (Node* node : nodeGrid[i][j]) {
					float dist = node->dist(relMousePos.x, relMousePos.y);
					if (dist < minDist) {
						minDist = dist;
						nearestNode = node;
					}
				}
			}
		}

		// handle window events
		sf::Event event;
		while (window.pollEvent(event))
		{
			// close
			if (event.type == sf::Event::Closed) {
				simRunning = false;
				doSimulation.notify_one();
				shouldExit = true;
				window.close();
			}
			// zoom
			else if (event.type == sf::Event::MouseWheelScrolled) {
				float zoom = 1.0f + event.mouseWheelScroll.delta * ZOOM_AMT * -1;
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
				switch (userNodesSelected) {
				case 0:
					userStartNode = nearestNode;
					if (userStartNode->getFillColor() != sf::Color::Cyan) {
						firstColor = userStartNode->getFillColor();
					}
					userStartNode->setFillColor(sf::Color::Cyan);
					#if ERROR_MODE == true
					std::cout << "User selected start " << userStartNode->id << std::endl;
					#endif
					userNodesSelected++;
					break;
				case 1:
					userEndNode = nearestNode;
					if (userStartNode != userEndNode && userEndNode->findPath(userStartNode, userPath, &userPathSize)) {
						if (userEndNode->getFillColor() != sf::Color::Cyan) {
							secondColor = userEndNode->getFillColor();
						}
						userEndNode->setFillColor(sf::Color::Cyan);
						#if ERROR_MODE == true
						std::cout << "User selected end " << userEndNode->id << std::endl;
						#endif
						for (char i = 0; i < userPathSize; i++) {
							userPathVertices.push_back(sf::Vertex(userPath[i].node->getPosition(), userPath[i].node->getFillColor()));
						}
						userPathVertexBuffer.create(userPathSize);
						userPathVertexBuffer.update(userPathVertices.data());
						userNodesSelected++;
					}
					break;
				case 2:
					#if ERROR_MODE == true
					std::cout << "User cleared selection" << std::endl;
					#endif
					userStartNode->setFillColor(firstColor);
					userEndNode->setFillColor(secondColor);
					userPathSize = 0;
					userPathVertices.clear();
					userPathVertexBuffer.update(userPathVertices.data());
					userNodesSelected = 0;
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
				// press semicolon to print useful information for debugging/performance analysis
				if (event.key.code == sf::Keyboard::Semicolon) {
					std::lock_guard<std::mutex> lock(citizensMutex);
					debugReport();
				}
				// press backspace to toggle "passive" citizen spawning
				if (event.key.code == sf::Keyboard::Backspace) {
					toggleSpawn = !toggleSpawn;
				}
			}
		}

		// apply arrow key pan
		panVelocity *= PAN_DECAY;
		panVelocity.x = std::max(std::min(panVelocity.x, MAX_PAN_VELOCITY), -MAX_PAN_VELOCITY);
		panVelocity.y = std::max(std::min(panVelocity.y, MAX_PAN_VELOCITY), -MAX_PAN_VELOCITY);
		view.move(panVelocity);

		window.clear();
		window.setView(view);
		window.draw(bg);

		// refresh text every TEXT_REFRESH_RATE frames
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
			text.setString(std::to_string(c) + " active citizens\n" + speedString + nearestNode->id + " [" + std::to_string(nearestNode->capacity) + "]");
		}

		if (drawTrains) {
			std::lock_guard<std::mutex> trainLock(trainsMutex);
			for (int i = 0; i < VALID_TRAINS; i++) {
				float newRadius = TRAIN_MIN_SIZE + trains[i].capacity / TRAIN_CAPACITY_FLOAT * (TRAIN_SIZE_DIFF);
				trains[i].updateRadius(newRadius);
				sf::Vector2f trainPosition = trains[i].getPosition();
				sf::Vector2f trainPositionNormalized = trainPosition - sf::Vector2f(newRadius, newRadius);
				sf::Color trainColor = trains[i].getFillColor();
				for (int j = 0; j < TRAIN_N_POINTS; j++) {
					int idx = i * TRAIN_N_POINTS * 3 + j * 3;
					trainVertices[idx] = sf::Vertex(trains[i].getPoint(j) + trainPositionNormalized, trainColor);
					trainVertices[idx+1] = sf::Vertex(trainPosition, trainColor);
					trainVertices[idx+2] = sf::Vertex(trains[i].getPoint((j + 1) % TRAIN_N_POINTS) + trainPositionNormalized, trainColor);
				}
			}

			window.draw(trainVertices);
		}

		if (drawNodes) {
			for (int i = 0; i < VALID_NODES; i++) {
				float newRadius = NODE_MIN_SIZE + std::min(NODE_CAPACITY, nodes[i].capacity) / NODE_CAPACITY_FLOAT * (NODE_SIZE_DIFF);
				nodes[i].updateRadius(newRadius);
				sf::Vector2f nodePosition = nodes[i].getPosition();
				sf::Vector2f nodePositionNormalized = nodePosition - sf::Vector2f(newRadius, newRadius);
				sf::Color nodeColor = nodes[i].getFillColor();
				for (int j = 0; j < NODE_N_POINTS; j++) {
					int idx = i * NODE_N_POINTS * 3 + j * 3;
					nodeVertices[idx] = sf::Vertex(nodes[i].getPoint(j) + nodePositionNormalized, nodeColor);
					nodeVertices[idx+1] = sf::Vertex(nodePosition, nodeColor);
					nodeVertices[idx+2] = sf::Vertex(nodes[i].getPoint((j + 1) % NODE_N_POINTS) + nodePositionNormalized, nodeColor);
				}
			}

			window.draw(nodeVertices);
		}

		if (drawLines) {
			if (userNodesSelected == 2) {
				window.draw(userPathVertexBuffer);
			}
			else {
				window.draw(linesVertexBuffer);
			}
		}

		window.setView(textView);
		window.draw(text);

		window.display();
	}
}

void pathfindingThread() {
	std::unique_lock<std::mutex> lock(pathsMutex);
	while (!shouldExit) {
		doPathfinding.wait(lock, [] {return !justDidPathfinding || customSpawnCitizens || shouldExit; });
		if (shouldExit) break;

		// spawn citizens at user request
		if (customSpawnCitizens) {
			int spawned = 0;
			for (int i = 0; i < CUSTOM_CITIZEN_SPAWN_AMT; i++) {
				Node* end = &nodes[rand() % VALID_NODES];
				if (nearestNode != end && citizens.add(nearestNode, end)) {
					handledCitizens++;
					spawned++;
				}
			}
			std::cout << "User spawned [" << CUSTOM_CITIZEN_SPAWN_AMT << "] at " << nearestNode->id << std::endl;
			customSpawnCitizens = false;
			doCustomCitizenSpawn.notify_one();
		}
		// spawn citizens using weighted-random node selection if spawning is enabled
		else if (toggleSpawn) {
			justDidPathfinding = true;

			#if CITIZEN_SPAWN_METHOD == 1
			// spawn a constant amount of citizens CITIZEN_SPAWN_AMT
			generateCitizens(CITIZEN_SPAWN_AMT);
			#else
			// spawn citizens up to a target amount TARGET_CITIZEN_COUNT
			generateCitizens(TARGET_CITIZEN_COUNT - (int)citizens.activeSize());
			#endif
		}
	}
}

void simulationThread() {
	SIM_SPEED = DEFAULT_SIM_SPEED;
	simRunning = false;

	// initialize stat recorders
	double timeElapsed = double(clock());
	activeCitizensStat.reserve(BENCHMARK_RESERVE);
	clockStat.reserve(BENCHMARK_RESERVE);
	simSpeedStat.reserve(BENCHMARK_RESERVE);

	CitizenThreadPool pool(NUM_CITIZEN_WORKER_THREADS);
	
	std::mutex simMutex;
	std::unique_lock<std::mutex> simLock(simMutex);
	while (!shouldExit) {
		// wait if paused
		doSimulation.wait(simLock, [] { return !simRunning; } );
		simTick++;

		#if BENCHMARK_MODE == true
		// benchmark mode disables rendering and exits after fixed amount of ticks
		if (simTick % STAT_RATE == 0) {
			std::cout << "\rProgress: " << float(simTick) / BENCHMARK_TICK_AMT * 100 << "%" << ", " << citizens.activeSize() << " active citizens" << std::flush;
		}
		if (simTick >= BENCHMARK_TICK_AMT) {
			std::cout << std::endl << "Benchmark concluded at tick " << simTick << std::endl;
			std::cout << "Handled " << handledCitizens << " citizens" << std::endl;
			shouldExit = true;
		}
		#endif

		// record statistics
		if (simTick % STAT_RATE == 0) {
			activeCitizensStat.push_back(citizens.activeSize());
			clockStat.push_back(double(clock()));
			size_t clockSize = clockStat.size();
			simSpeedStat.push_back(STAT_RATE / ((clockStat[clockSize-1] - clockStat[clockSize-2]) / CLOCKS_PER_SEC));
		}
		
		// ping pathfinding thread to spawn citizens
		if (simTick % CITIZEN_SPAWN_FREQ == 0 && toggleSpawn) {
			std::lock_guard<std::mutex> lock(pathsMutex);
			justDidPathfinding = false;
			doPathfinding.notify_one();
		}

		// run simulation on trains and citizens
		double handleClock = (double)clock();
		{
			std::lock_guard<std::mutex> lock(trainsMutex);
			float trainSpeed = TRAIN_SPEED * SIM_SPEED;
			for (int i = 0; i < VALID_TRAINS; i++) {
				trains[i].updatePositionAlongLine(trainSpeed);
			}
		}
		trainHandleTime = (double)clock() - handleClock;

		handleClock = (double)clock();
		{
			std::lock_guard<std::mutex> lock(citizensMutex);
			float citizenSpeed = CITIZEN_SPEED * SIM_SPEED;
			size_t chunkSize = citizens.activeSize() / NUM_CITIZEN_WORKER_THREADS + 1;

			for (int i = 0; i < NUM_CITIZEN_WORKER_THREADS; i++) {
				pool.enqueue([i, chunkSize, citizenSpeed]() {
					size_t start = i * chunkSize;
					size_t end = std::min(start + chunkSize, citizens.activeSize());
					for (size_t ind = start; ind < end; ++ind) {
						citizens.triggerCitizenUpdate(ind, citizenSpeed);
					}
					});
			}

			pool.waitForCompletion();
		}
		citizenHandleTime = (double)clock() - handleClock;
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

int main() {
	// initialize memory
	double progStartTime = double(clock());
	int initStatus = init();
	if (initStatus == AOK) {
		std::cout << "Simulation initialized successfully (" << (double(clock()) - progStartTime) / CLOCKS_PER_SEC << "s)" << std::endl << std::endl;
	} else {
		return initStatus;
	}

	// initialize threads
	std::thread renThread;
	#if BENCHMARK_MODE == true
	// disable rendering
	std::cout << "Simulation running in benchmark mode (" << BENCHMARK_TICK_AMT << " ticks)" << std::endl;
	std::cout << "Citizens spawn " << CITIZEN_SPAWN_AMT << "/" << CITIZEN_SPAWN_FREQ << " ticks, max " << MAX_CITIZENS << std::endl;
	#else
	// enable rendering
	renThread = std::thread(renderingThread);
	#endif

	std::thread simThread(simulationThread);
	std::thread pathThread(pathfindingThread);

	// exit
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
