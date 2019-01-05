#ifndef  __COMDEF_H
#define  __COMDEF_H

#include <stdio.h>
#include <errno.h>
#include <string>

#define Log(format, argv...) \
    printf((std::string("%s|%d|") + std::string(format) + "\n").c_str(), __FILE__, __LINE__, ##argv);

#define PerrorLog(func, format, argv...) { \
	perror(func); \
	printf((std::string("%s|%d|") + std::string(format) + "\n").c_str(), __FILE__, __LINE__, ##argv); \
}

#endif  // __COMDEF_H
