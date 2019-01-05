#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <string>
#include <algorithm>
using namespace std;
#include "comdef.h"
#include "comfunc.h"
#include "sysfunc.h"

const int iEventNum = 200;
char chReadBuf[1024];

int main(int argc, char const *argv[])
{
	int iEpfd = epoll_create(iEventNum);
	if(iEpfd < 0)
	{
		PerrorLog("epoll_create", "iEpfd: %d", iEpfd);
		exit(iEpfd);
	}
	int iTimerfd = timerfd_create(CLOCK_MONOTONIC, 0);
	if(iTimerfd < 0)
	{
		PerrorLog("timerfd_create", "iTimer: %d", iTimerfd);
		exit(iTimerfd);
	}
	struct itimerspec new_value;
    new_value.it_value.tv_sec = 2;
    new_value.it_value.tv_nsec = 0;
    new_value.it_interval.tv_sec = 1;
    new_value.it_interval.tv_nsec = 0;
    int iRes = timerfd_settime(iTimerfd, 0, &new_value, NULL);
    if(iRes < 0)
    {
    	PerrorLog("timerfd_settime", "iRes: %d", iRes);
		exit(iRes);
    }
    epoll_event epvTimer;
    epvTimer.events = EPOLLIN | EPOLLET;
    epvTimer.data.fd = iTimerfd;
    iRes = epoll_ctl(iEpfd, EPOLL_CTL_ADD, iTimerfd, &epvTimer);
    if(iRes < 0)
    {
    	PerrorLog("epoll_ctl", "iRes: %d", iRes);
		exit(iRes);
    }
    epoll_event epvEvents[iEventNum + 1];
    while(true)
    {
    	int iEvents = epoll_wait(iEpfd, epvEvents, iEventNum, 3 * 1000);
    	if(iEvents < 0)
    	{
    		PerrorLog("epoll_wait", "iEvents: %d", iEvents);
    		break;
    	}
    	if(iEvents == 0)
    	{
    		Log("no event util timeout, break");
    		break;
    	}
    	Log("ready event num: %d", iEvents);

    	for(int i = 0; i < iEvents; ++i)
    	{
    		Log("event: %d, fd: %d", epvEvents[i].events, epvEvents[i].data.fd);
    		if(epvEvents[i].data.fd == iTimerfd)
    		{
    			uint64_t ulTimeoutNum = 0;
    			iRes = ::read(iTimerfd, &ulTimeoutNum, sizeof(ulTimeoutNum));
    			if(iRes <= 0)
    			{
    				PerrorLog("read", "iRes: %d", iRes);
    				exit(iRes);
    			}
    			Log("now: %lu, ulTimeoutNum: %d", (long long)time(NULL), ulTimeoutNum);
    		}
    	}
    	sleep(2);			// ulTimeoutNum 会变为2
    	Log("sleep 2s.");
    }

    Log("finish, now: %d", (long long)time(NULL));
	return 0;
}
