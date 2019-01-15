#include "ae.h"
#include "anet.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <iostream>
#include <algorithm>
using namespace std;

#include "comdef.h"
#include "comfunc.h"

const int g_iEventSize = 10240;

int getTcpClient(const char *chServerIp, int iPort)
{
    sockaddr_in sinServer;
    sinServer.sin_family = AF_INET;
    sinServer.sin_port = htons(iPort);
    inet_aton(chServerIp, &(sinServer.sin_addr));

    int sockClient = ::socket(AF_INET, SOCK_STREAM, 0);
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
    iErrno = setNonBlock(sockClient);
    if(iErrno < 0)
    {
        Log("setNonBlock fail, ret: %d", iErrno);
        return iErrno;
    }
    return sockClient;
}

int getTimer(int64_t i64Second, int64_t i64NSec, int iTimerfd = -1)
{
    if(iTimerfd < 0)
    {
        iTimerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    }
    if(iTimerfd < 0)
    {
        PerrorLog("timerfd_create", "iTimerfd: %d", iTimerfd);
        return iTimerfd;
    }
    itimerspec itNewTimer;
    itNewTimer.it_value.tv_sec = i64Second;
    itNewTimer.it_value.tv_nsec = i64NSec;
    itNewTimer.it_interval.tv_sec = i64Second;
    itNewTimer.it_interval.tv_nsec = i64NSec;

    int iRes = timerfd_settime(iTimerfd, 0, &itNewTimer, NULL);
    if(iRes < 0)
    {
        PerrorLog("timerfd_settime", "iRes: %d", iRes);
        return iRes;
    }
    return iTimerfd;
}

void evSockCallback(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
{
    Log("evSockCallback begin, eventLoop: %p, fd: %d, clientData: %p, mask: %d", eventLoop, fd, clientData, mask);
    if(mask & AE_READABLE)
    {
        char chReadBuf[1024];
        // int iTotalLen = anetRead(fd, chReadBuf, sizeof(chReadBuf) - 1);
        int iTotalLen = ::read(fd, chReadBuf, sizeof(chReadBuf) - 1);
        
        if(iTotalLen <= 0)
        {
            Log("read finish");
            exit(1);
        }
        if(chReadBuf[iTotalLen - 1] != '\0')
        {
            chReadBuf[iTotalLen++] = '\0';
        }
        Log("chReadBuf: %s, iTotalLen: %d", chReadBuf, iTotalLen);
    }
    if(mask & AE_WRITABLE)
    {
        Log("client[%d] can write now", fd);
        sleep(1);
    }
}

void evStdinReadCallback(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
{
    Log("evStdinReadCallback begin, eventLoop: %p, fd: %d, clientData: %p, mask: %d", eventLoop, fd, clientData, mask);

    if(mask & AE_READABLE)
    {
        Log("fd[%d] can read now", fd);
        sleep(1);

        // char chReadBuf[1024];
        // // int iTotalLen = anetRead(fd, chReadBuf, sizeof(chReadBuf) - 1);
        // int iTotalLen = ::read(fd, chReadBuf, sizeof(chReadBuf) - 1);

        // if(iTotalLen > 0)
        // {
        //     chReadBuf[iTotalLen++] = '\0';
        // }
        // else
        // {
        //     Log("read from stdin error");
        //     exit(2);
        // }
        // Log("chReadBuf: %s", chReadBuf);

        // int iSockClient = *(int*)clientData;

        // int iWriteLen = anetWrite(iSockClient, chReadBuf, iTotalLen);
        // Log("write end, iWriteLen: %d", iWriteLen);
    }
    if(mask & AE_WRITABLE)
    {
        Log("fd[%d] can write now", fd);
        sleep(1);
    }
}

class CTest {
public:
static void evStdinWriteCallback(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
{
    Log("evStdinWriteCallback begin, eventLoop: %p, fd: %d, clientData: %p, mask: %d", eventLoop, fd, clientData, mask);
    
    if(mask & AE_READABLE)
    {
        Log("fd[%d] can read now", fd);
        sleep(1);
    }
    if(mask & AE_WRITABLE)
    {
        Log("fd[%d] can write now", fd);
        sleep(1);
    }
}

};

void evTimerCallback(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
{
    Log("evTimerCallback begin, eventLoop: %p, fd: %d, clientData: %p, mask: %d", eventLoop, fd, clientData, mask);

    if(mask & AE_READABLE)
    {
        uint64_t ulTimeoutNum = 0;
        ::read(fd, &ulTimeoutNum, sizeof(uint64_t));  //及时接收所有信息(猜测timer类fd只有uint64这8个字节的信息而已)

        timeval tvNow;
        gettimeofday(&tvNow, NULL);
        timeval *ptvLast = (timeval*)clientData;
        long long tDiff = (tvNow.tv_sec - ptvLast->tv_sec) * 1000000 + (tvNow.tv_usec - ptvLast->tv_usec);
        Log("ulTimeoutNum: %lu, now: %ld s, %ld us, last_time: %ld s, %ld us, diff: %ld us", 
            ulTimeoutNum, tvNow.tv_sec, tvNow.tv_usec, ptvLast->tv_sec, ptvLast->tv_usec, tDiff);

        aeDeleteFileEvent(eventLoop, fd, AE_READABLE);
        if(eventLoop->maxfd < 0)
        {
            aeStop(eventLoop);
            // aeDeleteEventLoop(eventLoop);
        }
    }
    if(mask & AE_WRITABLE)
    {
        Log("timer fd[%d] can write now", fd);
        sleep(1);
    }
}

void beforeSleepProc(struct aeEventLoop *eventLoop)
{
    Log("beforeSleepProc begin, maxfd: %d, timeEventNextId: %d", eventLoop->maxfd, eventLoop->timeEventNextId);
    if(eventLoop->maxfd < 0)
    {
        if(eventLoop->timeEventHead == NULL ||
            (eventLoop->timeEventHead->id == 0/* && eventLoop->timeEventHead->next == NULL*/))
        {
            Log("here stop");
            aeStop(eventLoop);
        }
    }
}

void evFinalizerProc(struct aeEventLoop *eventLoop, void *clientData)
{
    Log("evFinalizerProc begin, eventLoop: %p, clientData: %p", eventLoop, clientData);
    if(eventLoop->timeEventHead == NULL)
    {
        Log("eventLoop->timeEventHead == NULL");
        aeStop(eventLoop);
    }
}

int evTimerCb(struct aeEventLoop *eventLoop, long long id, void *clientData)
{
    Log("evTimerCb begin, eventLoop: %p, id: %d, clientData: %p", eventLoop, id, clientData);

    timeval tvNow;
    gettimeofday(&tvNow, NULL);
    timeval *ptvLast = (timeval*)clientData;
    long long tDiff = (tvNow.tv_sec - ptvLast->tv_sec) * 1000000 + (tvNow.tv_usec - ptvLast->tv_usec);
    Log("now: %ld s, %ld us, last_time: %ld s, %ld us, diff: %ld us", 
        tvNow.tv_sec, tvNow.tv_usec, ptvLast->tv_sec, ptvLast->tv_usec, tDiff);

    if(id == 0)
    {
        Log("event loop timeout reach, exit all");
        aeStop(eventLoop);
    }
    return AE_NOMORE;
}

int main()
{
    // int iFd = getTimer(179 / 1000, (uint64_t)179 * 1000 * 1000);
    // Log("iFd: %d", iFd);
    // ::close(iFd);
    // ::close(iFd);   //多次close无影响
    // return;
    
    timeval tvNow;
    gettimeofday(&tvNow, NULL);

    aeEventLoop *pEventBase = aeCreateEventLoop(g_iEventSize);
    aeCreateTimeEvent(pEventBase, 1200, evTimerCb, (void*)&tvNow, NULL);
    aeSetBeforeSleepProc(pEventBase, beforeSleepProc);

    long long lTimerId = 0;
    lTimerId = aeCreateTimeEvent(pEventBase, 1202, evTimerCb, (void*)&tvNow, NULL);
    Log("create timer %ld", lTimerId);
    // lTimerId = aeCreateTimeEvent(pEventBase, 80, evTimerCb, (void*)&tvNow, evFinalizerProc);
    // Log("create timer %ld", lTimerId);
    // lTimerId = aeCreateTimeEvent(pEventBase, 120, evTimerCb, (void*)&tvNow, evFinalizerProc);
    // Log("create timer %ld", lTimerId);

    // int iSockClient = getTcpClient("127.0.0.1", 9006);
    // aeCreateFileEvent(pEventBase, iSockClient, AE_READABLE, (aeFileProc*)evSockCallback, (void*)1);

    // int iRes = 0;
    // iRes = aeCreateFileEvent(pEventBase, STDIN_FILENO, AE_WRITABLE | AE_BARRIER, (aeFileProc*)CTest::evStdinWriteCallback, (void*)2);
    // Log("iRes: %d", iRes);
    // iRes = aeCreateFileEvent(pEventBase, STDIN_FILENO, AE_READABLE, (aeFileProc*)evStdinReadCallback, (void*)1);
    // Log("iRes: %d", iRes);
    // iRes = aeCreateFileEvent(pEventBase, STDIN_FILENO, AE_READABLE | AE_WRITABLE, (aeFileProc*)evStdinReadCallback, (void*)&iSockClient);
    // aeCreateFileEvent(pEventBase, STDIN_FILENO, AE_READABLE | AE_WRITABLE | AE_BARRIER, (aeFileProc*)evStdinReadCallback, (void*)&iSockClient);

    int iTimerfd = -1;
    int iMaxTimerFd = -1;

    for(int i = 0; i < 0; ++i)
    {
        // int iTimerfd = getTimer(0, 400 * 1000 * 1000);
        iTimerfd = getTimer((190 + i) / 1000, (uint64_t)199 % 1000 * 1000 * 1000, iTimerfd);
        iMaxTimerFd = std::max(iMaxTimerFd, iTimerfd);
        aeCreateFileEvent(pEventBase, iTimerfd, AE_READABLE, (aeFileProc*)evTimerCallback, (void*)&tvNow);  //timer fd没有写事件
    }

    // pEventBase->stop = 0;
    // while (!pEventBase->stop) {
    //     if (pEventBase->beforesleep != NULL)
    //         pEventBase->beforesleep(pEventBase);
    //     int iProcessNum = aeProcessEvents(pEventBase, AE_ALL_EVENTS|AE_CALL_AFTER_SLEEP);
    //     Log("----------------------------one process end, process_num: %d---------------------------------", iProcessNum);
    // }

    aeMain(pEventBase);     //循环调用aeProcessEvents(epoll_wait)
    // aeProcessEvents(pEventBase, AE_FILE_EVENTS | AE_CALL_AFTER_SLEEP);   //只调用一次epoll_wait

    int x;
    // cin >> x;

    return 0;
}
