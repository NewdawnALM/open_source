CC = g++

# OPT = -g --static -std=c++11
OPT = -g -std=c++11

INCLUDES = -I./common -I../install/include

LIB_DIR = -L../install/lib

LIBS = -levent

TARGET = $(patsubst %.cpp, %, $(wildcard *.cpp))

all: $(TARGET)
	@echo $(TARGET)

%: %.cpp
	$(CC) $(OPT) -o $@ $^ $(INCLUDES) $(LIB_DIR) $(LIBS)
	@echo ok

# simple_server: simple_server.cpp
# 	$(CC) $(OPT) -o $@ $^ $(INCLUDES) $(LIB_DIR) $(LIBS)
# 	@echo ok

# simple_client: simple_client.cpp
# 	$(CC) $(OPT) -o $@ $^ $(INCLUDES) $(LIB_DIR) $(LIBS)
# 	@echo ok

clean:
	rm -f *.o
	rm -f $(TARGET)
