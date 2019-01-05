#ifndef  __COMFUNC_H
#define  __COMFUNC_H

#include "comdef.h"
#include <fcntl.h>

inline int setNonBlock(int fd)
{
	int iFlag = ::fcntl(fd, F_GETFL, NULL);
	if(iFlag < 0)
	{
		PerrorLog("fcntl", "iFlag: %d, fd: %d", iFlag, fd);
		return iFlag;
	}
	if((iFlag & O_NONBLOCK) == 0)
	{
		int iRes = ::fcntl(fd, F_SETFL, iFlag | O_NONBLOCK);
		if(iRes < 0)
		{
			PerrorLog("fcntl", "iFlag: %d", iFlag);
			return iRes;
		}
	}
	return 0;
}

#endif  // __COMFUNC_H
