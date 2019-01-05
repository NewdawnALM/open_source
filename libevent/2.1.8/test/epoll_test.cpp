#include <stdlib.h>
#include <stdio.h> 
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <string>
#include <set>
using namespace std;
#include "comfunc.h"
#include "sysfunc.h"

const int iMaxEventFd = 100;
const int iBuffLen = 1024;
set<int> setClientFd;
char msg[iBuffLen + 1];

int getTcpSocket(int iPort, int iListenNum)
{
	int iRes = 0;
	int iServerSock = ::socket(AF_INET, SOCK_STREAM, 0);
	if(iServerSock <= 0)
	{
		PerrorLog("socket", "create socket fd error");
		return iServerSock;
	}
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(iPort);
	// Log("sin.sin_port: %u", sin.sin_port);
	
	iRes = ::bind(iServerSock, (sockaddr*)&sin, (socklen_t)sizeof(sin));
	if(iRes < 0)
	{
		PerrorLog("bind", "iRes: %d", iRes);
		return iRes;
	}
	iRes = ::listen(iServerSock, iListenNum);
	if(iRes < 0)
	{
		PerrorLog("listen", "iRes: %d", iRes);
		return iRes;
	}
	iRes = setNonBlock(iServerSock);
	if(iRes < 0)
	{
		return iRes;
	}
	return iServerSock;
}

int getTimer(int iSeconds = 1)
{
	int iTimerfd = timerfd_create(CLOCK_MONOTONIC, 0);
	if(iTimerfd < 0)
	{
		PerrorLog("timerfd_create", "iTimerfd: %d", iTimerfd);
		return iTimerfd;
	}
	itimerspec itNewTimer;
	itNewTimer.it_value.tv_sec = iSeconds;
	itNewTimer.it_value.tv_nsec = 0;
	itNewTimer.it_interval.tv_sec = iSeconds;
	itNewTimer.it_interval.tv_nsec = 0;

	int iRes = timerfd_settime(iTimerfd, 0, &itNewTimer, NULL);
	if(iRes < 0)
	{
		PerrorLog("timerfd_settime", "iRes: %d", iRes);
		return iRes;
	}
	return iTimerfd;
}

int main(int argc, char const *argv[])
{
	if(argc < 2)
    {
        printf("usage: %s [port]\n", argv[0]);
        exit(1);
    }
    int iPort = strtol(argv[1], NULL, 10);

    int iServerSock = getTcpSocket(iPort, 10);
    Log("iServerSock: %d", iServerSock);
    if(iServerSock < 0)
    {
    	Log("iServerSock[%d] invalid", iServerSock);
    	exit(2);
    }

    int iEpfd = epoll_create(iMaxEventFd);
    if(iEpfd < 0)
    {
    	PerrorLog("epoll_create", "iEpfd: %d", iEpfd);
    	exit(iEpfd);
    }
    int iRes = 0;
	epoll_event epv;
	epv.events = EPOLLIN;
	epv.data.fd = iServerSock;
    iRes = epoll_ctl(iEpfd, EPOLL_CTL_ADD, iServerSock, &epv);
    if(iRes < 0)
    {
    	PerrorLog("epoll_ctl", "iRes: %d", iRes);
    	exit(iRes);
    }
    int iTimerfd = getTimer(1);
    if(iTimerfd < 0)
    {
    	exit(iTimerfd);
    }
    epoll_event epvTimer;
    epvTimer.events = EPOLLIN | EPOLLET;	//设置边缘触发，否则事件发生时如果不及时read会不断被epoll_wait立刻获取返回，造成死循环
    epvTimer.data.fd = iTimerfd;
    iRes = epoll_ctl(iEpfd, EPOLL_CTL_ADD, iTimerfd, &epvTimer);
    if(iRes < 0)
    {
    	PerrorLog("epoll_ctl", "iRes: %d", iRes);
    	exit(iRes);
    }
    timeval tvStart;
    assert(gettimeofday(&tvStart, NULL) == 0);

    epoll_event epEvents[iMaxEventFd];

    while(true)
    {
    	int iEvents = epoll_wait(iEpfd, epEvents, iMaxEventFd, 120 * 1000);
    	Log("ready event num: %d, current_time: %lu", iEvents, time(NULL));
    	if(iEvents < 0)
    	{
    		PerrorLog("epoll_wait", "iEvents: %d", iEvents);
    		break;
    	}
    	else if(iEvents == 0)
    	{
    		Log("no event util timeout, close server.");
    		break;
    	}
    	for(int idx = 0; idx < iEvents; ++idx)
    	{
    		Log("event: %d, fd: %d", epEvents[idx].events, epEvents[idx].data.fd);
    		if(epEvents[idx].data.fd == iServerSock)
    		{
    			sockaddr_in sinClient;
    			socklen_t slen = sizeof(sinClient);
    			int iClientFd = ::accept(iServerSock, (sockaddr*)&sinClient, &slen);
    			if(iClientFd < 0)
    			{
    				PerrorLog("accept", "iClientFd: %d", iClientFd);
    				exit(iClientFd);
    			}
    			Log("accept a client: %d", iClientFd);
    			setClientFd.insert(iClientFd);

    			epoll_event epvClient;
    			epvClient.events = EPOLLIN;
    			epvClient.data.fd = iClientFd;
    			iRes = epoll_ctl(iEpfd, EPOLL_CTL_ADD, iClientFd, &epvClient);
    			if(iRes < 0)
    			{
    				PerrorLog("epoll_ctl", "iRes: %d, iClientFd: %d", iRes, iClientFd);
    				Close(iClientFd);
    			}
    		}
    		else if(epEvents[idx].data.fd == iTimerfd)
    		{
    			timeval tvNow;
    			gettimeofday(&tvNow, NULL);
    			long sec_diff = tvNow.tv_sec - tvStart.tv_sec;
    			long usec_diff = tvNow.tv_usec - tvStart.tv_usec;
    			if(usec_diff < 0)
    			{
    				--sec_diff;
    				usec_diff += 1000000;
    			}
    			uint64_t ulTimeoutNum = 0;
    			::read(iTimerfd, &ulTimeoutNum, sizeof(uint64_t));	//及时接收所有信息(猜测timer类fd只有uint64这8个字节的信息而已)

    			Log("cost time: %lu.%03lu, timeout_num: %lu", sec_diff, usec_diff / 1000, ulTimeoutNum);
    		}
    		else if(setClientFd.find(epEvents[idx].data.fd) != setClientFd.end())
    		{
    			int iClientFd = epEvents[idx].data.fd;
    			int iSize = ::read(iClientFd, msg, iBuffLen);
    			if(iSize <= 0)
    			{
    				if(iSize == 0)
    				{
    					Log("read size is 0, close client[%d]", iClientFd);
    				}
    				else
    				{
    					PerrorLog("read", "close client[%d]", iClientFd);
    				}
    				epoll_event epvClient{0, {0} };
    				int iRes = epoll_ctl(iEpfd, EPOLL_CTL_DEL, iClientFd, &epvClient);
    				if(iRes < 0)
    				{
    					PerrorLog("epoll_ctl", "iRes: %d, iClientFd: %d", iRes, iClientFd);
    				}
    				Close(iClientFd);
    				setClientFd.erase(iClientFd);
    			}
    			else
    			{
	    			msg[iSize++] = '\0';
	    			char chSendMsg[iBuffLen + 100];
	    			sprintf(chSendMsg, "recv client[%d] msg: %s", iClientFd, msg);
	    			Log(chSendMsg);
	    			int iSize = ::write(iClientFd, chSendMsg, strlen(chSendMsg));
	    			Log("write size: %d", iSize);
    			}
    		}
    	}
    	puts("");
    	// sleep(30);	//模拟业务处理耗时
    }
    Close(iServerSock);
    Close(iEpfd);

    Log("finish time: %lu", time(NULL));
	return 0;
}
