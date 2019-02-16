// #include "../redis-5.0.0/src/sds.h"
#include <iostream>
using namespace std;

struct __attribute__ ((__packed__)) sdshdr5 {
    unsigned char flags; /* 3 lsb of type, and 5 msb of string length */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr8 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr16 {
    uint16_t len; /* used */
    uint16_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr32 {
    uint32_t len; /* used */
    uint32_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr64 {
    uint64_t len; /* used */
    uint64_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};


struct CPack
{
	// char chBuf[];	//length = 0
};

int main(int argc, char const *argv[])
{
	cout << sizeof(sdshdr5) << "\n";
	cout << sizeof(sdshdr8) << "\n";
	cout << sizeof(sdshdr16) << "\n";
	cout << sizeof(sdshdr32) << "\n";
	cout << sizeof(sdshdr64) << "\n";

	cout << sizeof(CPack) << "\n";
	cout << sizeof(char[0]) << "\n";
	return 0;
}

// test svn commit $file1 $file2...