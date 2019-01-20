CC = g++

# OPT = -g --static -std=c++11
OPT = -g -std=c++11

INCLUDES = -I./common -I../install/include

LIB_SRC_DIR = ../install/src

# LIB_DIR = -L../install/lib

# LIBS = -levent

TARGET = $(patsubst %.cpp, %, $(wildcard *.cpp))
TARGET += time_event_2 time_event_3

all: $(TARGET)
	@echo $(TARGET)

time_event_3: time_event.cpp $(LIB_SRC_DIR)/ae3.cpp
	$(CC) $(OPT) -DUSE_AE3 -o $@ $^ $(INCLUDES) $(LIB_DIR) $(LIBS)
	@echo ok

time_event_2: time_event.cpp $(LIB_SRC_DIR)/ae2.cpp
	$(CC) $(OPT) -DUSE_AE2 -o $@ $^ $(INCLUDES) $(LIB_DIR) $(LIBS)
	@echo ok

%: %.cpp $(LIB_SRC_DIR)/ae.c
	$(CC) $(OPT) -o $@ $^ $(INCLUDES) $(LIB_DIR) $(LIBS)
	@echo ok

clean:
	rm -f *.o
	rm -f $(TARGET)
