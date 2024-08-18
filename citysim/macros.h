#pragma once

// Debugging
#define AOK							0
#define ERROR_OPENING_FILE			1
#define BENCHMARK_MODE				0
#define BENCHMARK_TICK_DURATION		50000
#define BENCHMARK_STAT_RATE			1000
#define BENCHMARK_RESERVE			BENCHMARK_TICK_DURATION / BENCHMARK_STAT_RATE

// Graphics
#define WINDOW_X					1000
#define WINDOW_Y					1200
#define WINDOW_SCALE				0.5f
#define WINDOW_X_SCALE				1.25f
#define WINDOW_Y_SCALE				1.0f
#define WINDOW_X_OFFSET				100
#define WINDOW_Y_OFFSET				-150
#define ZOOM_SCALE					0.1f
#define ZOOM_MIN					0.5f
#define ZOOM_MAX					4.0f
#define TRAIN_MIN_SIZE				5.0f
#define TRAIN_MAX_SIZE				10.0f
#define TRAIN_SIZE_DIFF				TRAIN_MAX_SIZE - TRAIN_MIN_SIZE
#define TRAIN_N_POINTS				20
#define NODE_MIN_SIZE				2.5f
#define NODE_MAX_SIZE				7.5f
#define NODE_SIZE_DIFF				NODE_MAX_SIZE - NODE_MIN_SIZE
#define NODE_N_POINTS				10
#define CITIZEN_SIZE				4.0f
#define CITIZEN_N_POINTS			4
#define TEXT_REFRESH_RATE			10 // every n frames

// Simulation size
#define MAX_LINES					32
#define MAX_NODES					512
#define MAX_TRAINS					1024
#define MAX_CITIZENS				262144

// Simulation pacing
#define DEFAULT_SIM_SPEED			5.0f
#define MIN_SIM_SPEED				0.5f
#define MAX_SIM_SPEED				10.0f
#define SIM_SPEED_INCR				0.25f
#define DEFAULT_TRAIN_STOP_SPACING	8
#define DISTANCE_SCALE				128

// File loading
#define STATIONS_CSV_NUM_COLUMNS	6
#define GEOM_CSV_NUM_COLUMNS		9

// Node and Train status flags
#define STATUS_DESPAWNED			0
#define STATUS_DESPAWNED_ERR		0 // should have the same value as STATUS_DESPAWNED
#define STATUS_SPAWNED				1
#define STATUS_IN_TRANSIT			2
#define STATUS_AT_STOP				3
#define STATUS_TRANSFER				4
#define STATUS_WALK					5
#define STATUS_FORWARD				1
#define STATUS_BACK					0
#define STATUS_EXISTS				1
#define STATUS_HIGHLIGHTED			2

// Line
#define LINE_PATH_SIZE				64
#define WALK_LINE_ID_STR			"WLK"

// Node
#define NODE_ID_SIZE				36
#define N_NEIGHBORS					24
#define N_TRAINS					8
#define TRANSFER_WEIGHT				16
#define TRANSFER_MAX_DIST			2
#define TRANSFER_PENALTY			24
#define TRANSFER_PENALTY_MULTIPLIER	1.5f
#define STOP_PENALTY				2
#define NODE_CAPACITY				192

// Train
#define TRAIN_SPEED					4
#define TRAIN_CAPACITY				256
#define TRAIN_STOP_THRESH			256 * TRAIN_SPEED

// Citizen
#define CITIZEN_SPEED				1
#define	CITIZEN_TRANSFER_THRESH		64 * CITIZEN_SPEED
#define CITIZEN_SPAWN_INIT			32000
#define CITIZEN_SPAWN_FREQ			1024
#define CITIZEN_SPAWN_MAX			512
#define TARGET_CITIZEN_COUNT		32000
#define CITIZEN_RANDOMIZE_SPAWN_AMT	0
#define N_NODES						64
#define CITIZEN_DESPAWN_THRESH		131072 * CITIZEN_SPEED
#define CITIZEN_PATH_SIZE			64
#define PATH_CACHE_SIZE				1024
#define CUSTOM_CITIZEN_SPAWN		256

// Simulation pacing
#define NUM_THREADS					3