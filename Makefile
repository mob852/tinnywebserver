CC = g++
CFLAGS = -Wall -g -std=c++11
TARGET = server

SRCDIR = src
SRC = $(SRCDIR)/main.cpp $(SRCDIR)/server.cpp $(SRCDIR)/epoll.cpp $(SRCDIR)/utils.cpp $(SRCDIR)/ThreadPool.cpp $(SRCDIR)/logger.cpp
OBJ = $(SRC:.cpp=.o)

INCLUDES = -I$(SRCDIR)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) -pthread

%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
