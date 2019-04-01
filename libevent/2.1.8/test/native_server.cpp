#include <cstdio>
#include <cstdlib>
#include <string.h>
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
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

#define Log(format, argv...) \
    printf((string("%d|%s|%d|") + string(format) + "\n").c_str(), getpid(), __FILE__, __LINE__, ##argv);

int main(int argc, char const *argv[])
{
	int iSock = ::socket(AF_INET, SOCK_STREAM, 0);
	assert(iSock > 0);

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(9005);
	assert(::bind(iSock, (sockaddr*)&sin, (socklen_t)sizeof(sin)) >= 0);

	assert(::listen(iSock, 5) >= 0);

	//先accept再fork其实不太好，因为accept只有一个进程/线程，当连接数太多时accept就堵了...
	while(true)
	{
		sockaddr sockClient;
		socklen_t socklen = sizeof(sockClient);
		int iClientSock = ::accept(iSock, &sockClient, &socklen);
		Log("accept client[%d]", iClientSock);

		pid_t pid = fork();
		if(pid < 0)
		{
			Log("fork fail");
			exit(1);
		}
		else if(pid == 0)	//子进程
		{
			pid_t pidChild = fork();	//二次fork避免僵尸进程
			if(pidChild < 0)
			{
				Log("child fork fail");
				exit(1);
			}
			if(pidChild == 0)	//孙子进程处理read/write
			{
				Log("grand son init");
				while(true)
				{
					char chBuf[1024];
					int iSize = ::read(iClientSock, chBuf, sizeof(chBuf));
					chBuf[iSize] = '\0';
					Log("iSize: %d, chBuf: %s", iSize, chBuf);

					if(iSize <= 0 || strncmp(chBuf, "bye", 3) == 0)
					{
						assert(::close(iClientSock) == 0);
						// break;
						exit(0);
					}
					char chReply[1024];
					sprintf(chReply, "server recv[%s], size[%d]", chBuf, iSize);
					iSize = ::write(iClientSock, chReply, strlen(chReply));
					Log("write finish, iSize: %d", iSize);
				}
			}
			else 	//子进程退出
			{
				assert(::close(iClientSock) == 0);	 //子进程也要关闭，因为close是引用计数的方式
				Log("child exit");
				exit(0);
			}
		}
		else 	// pid > 0，父进程
		{
			assert(::close(iClientSock) == 0);	 //父进程也要关闭，因为close是引用计数的方式
			Log("close client[%d]", iClientSock);
			int iStat = 0;
			pid_t pid_wait = wait(&iStat);
			Log("child[%d] exit, stat[%d]", pid_wait, iStat);
		}
	}
	
	//先fork再accept模型，下面实现不太正确，更多详见nginx
	// while(true)
	// {
	// 	sockaddr sockClient;
	// 	socklen_t socklen = sizeof(sockClient);

	// 	pid_t pid = fork();
	// 	if(pid < 0)
	// 	{
	// 		Log("fork fail");
	// 		exit(1);
	// 	}
	// 	else if(pid == 0)
	// 	{
	// 		Log("I am child");
	// 	}
	// 	else
	// 	{
	// 		Log("I am parent");
	// 	}

	// 	int iClientSock = ::accept(iSock, &sockClient, &socklen);
	// 	Log("accept client[%d]", iClientSock);

	// 	if(pid == 0)	//子进程处理read/write
	// 	{
	// 		while(true)
	// 		{
	// 			char chBuf[1024];
	// 			int iSize = ::read(iClientSock, chBuf, sizeof(chBuf));
	// 			chBuf[iSize] = '\0';
	// 			Log("iSize: %d, chBuf: %s", iSize, chBuf);

	// 			if(iSize <= 0 || strncmp(chBuf, "bye", 3) == 0)
	// 			{
	// 				assert(::close(iClientSock) == 0);
	// 				// break;
	// 				exit(0);
	// 			}
	// 			char chReply[1024];
	// 			sprintf(chReply, "server recv[%s], size[%d]", chBuf, iSize);
	// 			iSize = ::write(iClientSock, chReply, strlen(chReply));
	// 			Log("write finish, iSize: %d", iSize);
	// 		}
	// 	}
	// 	else 	// pid > 0，父进程
	// 	{
	// 		// Log("accept client[%d]", iClientSock);
	// 		assert(::close(iClientSock) == 0);	 //父进程也要关闭，因为close是引用计数的方式
	// 		Log("close client[%d]", iClientSock);
	// 	}
	// }

	return 0;
}