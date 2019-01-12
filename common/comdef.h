#ifndef  __COMDEF_H
#define  __COMDEF_H

#include <stdio.h>
#include <errno.h>
// #include <string>

#define Log(format, argv...) \
{ \
	printf("%s|%d|", __FILE__, __LINE__); \
    printf(format, ##argv); \
    putchar('\n'); \
}

#define PerrorLog(func, format, argv...) \
{ \
	perror(func); \
	printf("%s|%d|", __FILE__, __LINE__); \
    printf(format, ##argv); \
    putchar('\n'); \
}

#endif  // __COMDEF_H
