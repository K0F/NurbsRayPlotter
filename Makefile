TARGET   = nurbsRayPlotter
CC       = gcc
CXX      = g++
CFLAGS   = -Wall -O2
CXXFLAGS = -Wall -O2 -std=c++11 -Wno-unknown-pragmas

# Path to the submodule folder
ON_DIR   = ./external/opennurbs

# openNURBS needs to find its own headers, and so does our bridge
INCLUDES = -I. -I$(ON_DIR)

# Linker: Point to the folder where libopennurbs.a was just built
LDFLAGS  = -L$(ON_DIR) ./external/opennurbs/libopennurbs_public.a -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

SRCS_C   = main.c
SRCS_CPP = geometry_bridge.cpp
OBJS     = main.o geometry_bridge.o

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

main.o: main.c
	$(CC) $(CFLAGS) $(INCLUDES) -c main.c -o main.o

geometry_bridge.o: geometry_bridge.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c geometry_bridge.cpp -o geometry_bridge.o

clean:
	rm -f *.o $(TARGET)
