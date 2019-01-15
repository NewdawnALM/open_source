CC = g++

OPT = -O3 -std=c++11

AR = ar

INCLUDES = -I../include -I../../test/common

TARGET = ../lib/libae.a

OBJS = $(patsubst %.c, %.o, $(wildcard *.c))

%.o: %.c
	$(CC) $(OPT) -o $@ -c $^ $(INCLUDES)

$(TARGET): $(OBJS)
	$(AR) -srv $@ $^

all: $(TARGET)
	@echo $(TARGET)

clean:
	rm -f *.o
	rm -f $(TARGET)

