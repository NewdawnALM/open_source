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
#include <netinet/in.h>
#include "event.h"

#define Log(format, argv...) \
    printf((string("%s|%d|") + string(format) + "\n").c_str(), __FILE__, __LINE__, ##argv);


evutil_socket_t getTcpServer(int iPort, int iListenNum)
{
	evutil_socket_t sockServer = ::socket(AF_INET, SOCK_STREAM, 0);
	if(sockServer < 0)
	{
		return sockServer;
	}
	int iErrno = evutil_make_listen_socket_reuseable(sockServer);
	if(iErrno != 0)
	{
		perror("make_socket_reuseable");
		return iErrno;
	}
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(iPort);

	iErrno = ::bind(sockServer, (struct sockaddr *)&sin, (socklen_t)sizeof(sin));
	if(iErrno < 0)
	{
		perror("bind");
		evutil_closesocket(sockServer);
		return iErrno;
	}
	iErrno = ::listen(sockServer, iListenNum);
	if(iErrno < 0)
	{
		perror("listen");
		evutil_closesocket(sockServer);
		return iErrno;
	}
	iErrno = evutil_make_socket_nonblocking(sockServer);
	if(iErrno != 0)
	{
		return iErrno;
	}
	return sockServer;
}

void cbRead(evutil_socket_t sockClient, short sEvent, void *arg)
{
	Log("cbRead begin, sock: %d, event: %d", sockClient, sEvent);

	char chBuf[4096];
	int iSize = ::read(sockClient, chBuf, sizeof(chBuf));

	if(iSize <= 0)
	{
		if(iSize < 0)
		{
			printf("size: %d, errno: %d\n", iSize, errno);
            perror("some error happen when read");
		}
		Log("iSize: %d", iSize);
		event_free((event *)arg);
		evutil_closesocket(sockClient);
		return;
	}
	chBuf[iSize] = '\0';
	Log("server recv: %s", chBuf);

	iSize = ::write(sockClient, chBuf, iSize + 1);
	Log("write finish, size: %d", iSize);
}

void cbAccept(evutil_socket_t sockServer, short sEvent, void *arg)
{
	Log("cbAccept begin, sock: %d, event: %d", sockServer, sEvent);
	sockaddr_in sinClient;
	socklen_t sLen = sizeof(sinClient);
	evutil_socket_t sockClient = ::accept(sockServer, (sockaddr*)&sinClient, &sLen);
	Log("accept client %d", sockClient);

	int iErrno = evutil_make_socket_nonblocking(sockClient);
	if(iErrno != 0)
	{
		Log("make socket nonblocking fail");
		evutil_closesocket(sockClient);
		return;
	}
	event_base *base = (event_base*)arg;
	event *evRead = event_new(NULL, -1, 0, NULL, NULL);
	event_assign(evRead, base, sockClient, EV_READ | EV_PERSIST, cbRead, evRead);
	event_add(evRead, NULL);
}

int main(int argc, char const *argv[])
{
	if(argc < 2)
    {
        printf("usage: %s [port]\n", argv[0]);
        exit(1);
    }
    int iPort = strtol(argv[1], NULL, 10);
	evutil_socket_t sockServer = getTcpServer(iPort, 10);
	Log("server socket fd: %d", sockServer);

	if(sockServer < 0)
	{
		Log("server socket init error");
		exit(2);
	}
	event_base *base = event_base_new();
	//创建一个监听accept读的事件
	event *evAccept = event_new(base, sockServer, EV_READ | EV_PERSIST, cbAccept, (void *)base);
	event_add(evAccept, NULL);
	event_base_dispatch(base);

	Log("event_base_dispatch finish");
	return 0;
}
