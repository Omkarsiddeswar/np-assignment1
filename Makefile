CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall
SRCS = clientmain.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = client

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean