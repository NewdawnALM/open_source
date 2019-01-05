#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
using namespace std;
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "event.h"

#define Log(format, argv...) \
    printf((string("%s|%d|") + string(format) + "\n").c_str(), __FILE__, __LINE__, ##argv);


evutil_socket_t getTcpClient(const char *chServerIp, int iPort)
{
	sockaddr_in sinServer;
	sinServer.sin_family = AF_INET;
	sinServer.sin_port = htons(iPort);
	inet_aton(chServerIp, &(sinServer.sin_addr));

	evutil_socket_t sockClient = ::socket(AF_INET, SOCK_STREAM, 0);
	if(sockClient < 0)
	{
		perror("socket");
		return sockClient;
	}
	int iErrno = 0;
	iErrno = ::connect(sockClient, (sockaddr*)&sinServer, sizeof(sinServer));
	if(iErrno < 0)
	{
		perror("connect");
		return iErrno;
	}
	iErrno = evutil_make_socket_nonblocking(sockClient);
	if(iErrno < 0)
	{
		Log("make_socket_nonblocking fail, ret: %d", iErrno);
		return iErrno;
	}
	return sockClient;
}

void cbSockRead(evutil_socket_t sockClient, short sEvent, void *arg)
{
	char chBuf[4096];
	int iSize = ::read(sockClient, chBuf, sizeof(chBuf) - 1);

	if(iSize == 0)
	{
		Log("close client: %d", sockClient);
		evutil_closesocket(sockClient);
		event_free((event*)arg);
		return;
	}
	if(iSize < 0)
	{
		perror("read");
		event_free((event*)arg);
		evutil_closesocket(sockClient);
		return;
	}
	chBuf[iSize] = '\0';
	Log("recv: %s", chBuf);
}

//没有检测sockClient是否已关闭
void cbStdinRead(evutil_socket_t fdStdin, short sEvent, void *arg)
{
	char chBuf[4096];
	int iSize = ::read(fdStdin, chBuf, sizeof(chBuf) - 1);
	if(iSize <= 0)
	{
		perror("read");
		return;
	}
	evutil_socket_t sockClient = *((evutil_socket_t*)arg);
	Log("sockClient: %d", sockClient);
	
	int iWSize = ::write(sockClient, chBuf, iSize);
	if(iWSize <= 0)
	{
		perror("write");
		return;
	}
	Log("send finish, iWSize: %d", iWSize);
}

void cbTimeOut(evutil_socket_t fdTimeOut, short sEvent, void *arg)
{
	timeval tvNow;
	gettimeofday(&tvNow, NULL);
	timeval *ptvLast = (timeval*)arg;
	long long tDiff = (tvNow.tv_sec - ptvLast->tv_sec) * 1000000 + (tvNow.tv_usec - ptvLast->tv_usec);
	Log("fdTimeOut: %d, sEvent: %d, now: %ld s, %ld us, last_time: %ld s, %ld us, diff: %ld us", 
		fdTimeOut, sEvent, tvNow.tv_sec, tvNow.tv_usec, ptvLast->tv_sec, ptvLast->tv_usec, tDiff);
}

int main(int argc, char const *argv[])
{
	if(argc < 3)
	{
		Log("usage: %s [ip] [port]", argv[0]);
		exit(1);
	}
	evutil_socket_t sockClient = getTcpClient(argv[1], atoi(argv[2]));

	if(sockClient < 0)
	{
		Log("getTcpClient fail");
		exit(2);
	}
	Log("getTcpClient finish, sockClient: %d", sockClient);

	event_base *base = event_base_new();

	event *evSockRead = event_new(NULL, -1, 0, NULL, NULL);
	event_assign(evSockRead, base, sockClient, EV_READ | EV_PERSIST, cbSockRead, (void *)evSockRead);
	event_add(evSockRead, NULL);


	timeval tvNow;
	gettimeofday(&tvNow, NULL);

	event *evTimeOut = evtimer_new(base, cbTimeOut, (void*)&tvNow);	//创建一个超时事件
	timeval tvInterval{0, 0};
	evtimer_add(evTimeOut, &tvInterval);	//1.25s后触发超时事件

	event evTimeOut_2;
	evtimer_assign(&evTimeOut_2, base, cbTimeOut, (void*)&tvNow);	//这两行等价于上面的
	timeval tvInterval_2{0, 0};
	// timeval tvInterval{1, 250 * 1000};
	evtimer_add(&evTimeOut_2, &tvInterval_2);	//1.25s后触发超时事件
	


	event *evStdinRead = event_new(base, STDIN_FILENO, EV_READ | EV_PERSIST, cbStdinRead, (void*)&sockClient);
	event_add(evStdinRead, NULL);

	event_base_dispatch(base);
	Log("event_base_dispatch finish");

	return 0;
}
