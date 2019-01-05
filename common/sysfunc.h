#ifndef  __SYSFUNC_H
#define  __SYSFUNC_H

#include "comdef.h"
#include <unistd.h>

inline void Close(int fd)
{
	int iRes = ::close(fd);
    if(iRes < 0)
    {
    	PerrorLog("close", "fd: %d, iRes: %d", fd, iRes);
    	exit(iRes);
    }
}

#endif  // __SYSFUNC_H