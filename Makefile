# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Icitysim -pthread
LDFLAGS = -lsfml-graphics -lsfml-window -lsfml-system

# Source files
SRCS = citysim/sim.cpp citysim/node.cpp citysim/train.cpp citysim/citizen.cpp citysim/util.cpp citysim/pathcache.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = citysim_app

# Default rule
all: $(TARGET)

# Linking rule
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compilation rule
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
