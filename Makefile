# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Icitysim -pthread
DEBUGFLAGS = -g -O0 -DDEBUG
LDFLAGS = -lsfml-graphics -lsfml-window -lsfml-system

# Source files
SRCS = citysim/sim.cpp citysim/node.cpp citysim/train.cpp citysim/citizen.cpp citysim/util.cpp citysim/pathcache.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)
DEBUG_OBJS = $(SRCS:.cpp=.debug.o)

# Executable name
TARGET = citysim_app
DEBUG_TARGET = citysim_app_debug

# Default rule
all: $(TARGET)

# Linking rule
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Debug linking rule
$(DEBUG_TARGET): $(DEBUG_OBJS)
	$(CXX) $(DEBUG_OBJS) -o $(DEBUG_TARGET) $(LDFLAGS)

# Compilation rule
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Debug compilation rule
%.debug.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEBUGFLAGS) -c $< -o $@

# Debug rule
debug: $(DEBUG_TARGET)

# Clean rule
clean:
	rm -f $(OBJS) $(DEBUG_OBJS) $(TARGET) $(DEBUG_TARGET)

# Run rule
run:
	./$(TARGET)

# Debug run rule
debug-run: debug
	gdb ./$(DEBUG_TARGET)

.PHONY: all debug clean run debug-run
