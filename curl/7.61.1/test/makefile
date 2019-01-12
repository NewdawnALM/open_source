CC = g++

# STATIC = -static
DEBUG = -g
CPP_VER = -std=c++11

OPT = $(STATIC) $(DEBUG) $(CPP_VER) -O3 -DCURL_STATICLIB

# OPEN_SOURCE_ROOT = /home/newdawn/ALM/open_source
OPEN_SOURCE_ROOT = /data/home/mytest/open_source

COMMON_INCLUDE = $(OPEN_SOURCE_ROOT)/common

CURL_ROOT = $(OPEN_SOURCE_ROOT)/curl/7.61.1/install
CURL_INCLUDE = $(CURL_ROOT)/include
CURL_LIBDIR = $(CURL_ROOT)/lib
# CURL_LIBDIR = /data/home/mytest/wzq_proj/branches/app/wzq_activity_logic_server_V3.0D0063/comm/httpclient/curl/lib

OPENSSL_ROOT = $(OPEN_SOURCE_ROOT)/openssl/1.1.0i/install
OPENSSL_LIBDIR = $(OPENSSL_ROOT)/lib

LIBEVENT_ROOT = $(OPEN_SOURCE_ROOT)/libevent/2.1.8/install
LIBEVENT_INCLUDE = $(LIBEVENT_ROOT)/include
LIBEVENT_LIBDIR = $(LIBEVENT_ROOT)/lib

REDIS_ROOT = $(OPEN_SOURCE_ROOT)/redis/5.0.0/install
REDIS_INCLUDE = $(REDIS_ROOT)/include
REDIS_LIBDIR = $(REDIS_ROOT)/lib

INCLUDES = -I$(COMMON_INCLUDE) -I$(CURL_INCLUDE) -I$(LIBEVENT_INCLUDE) -I$(REDIS_INCLUDE)

# LIB_DIR = -L$(CURL_LIBDIR) -L$(OPENSSL_LIBDIR) -L$(LIBEVENT_LIBDIR) -L$(REDIS_LIBDIR)
LIB_DIR = -L$(CURL_LIBDIR) -L$(LIBEVENT_LIBDIR) -L$(REDIS_LIBDIR)

LIBS = -lcurl -lssl -lcrypto -ldl -levent -lae -pthread -lz

TARGET = $(patsubst %.cpp, %, $(wildcard *.cpp))

all: $(TARGET)
	@echo $(TARGET)

%: %.cpp
	$(CC) $(OPT) -o $@ $^ $(INCLUDES) $(LIB_DIR) $(LIBS)
	@echo ok

clean:
	rm -f *.o
	rm -f $(TARGET)
