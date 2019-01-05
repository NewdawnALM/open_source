#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/timerfd.h>

#include "curl/curl.h"
#include "ae.h"
#include "anet.h"
#include "comdef.h"
#include "comfunc.h"
#include "env_config.h"

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <assert.h>
using namespace std;

const int64_t g_i64TimeOut = 4000;  //ms

#define Curl_easy_setopt(curl, option, argv...) \
    assert(curl_easy_setopt(curl, option, ##argv) == CURLE_OK);

#define Curl_multi_setopt(curl, option, argv...) \
    assert(curl_multi_setopt(curl, option, ##argv) == CURLM_OK);


int getTimer(int64_t i64Second, int64_t i64NSec)
{
    int iTimerfd = timerfd_create(CLOCK_MONOTONIC, 0);

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


class CMultiInfo
{
public:
    CURLM *m_curlm;
    int m_iRunning;
    aeEventLoop *m_evpBase;
    // event_base *m_evpBase;
    int m_iMultiTimerFd;
    // event *m_evpMultiTimer;
};

class CConnInfo
{
public:
    string m_strUrl;
    string m_strHost;
    int64_t m_i64TimeOut;
    CURL *m_curl;
    string m_strResp;
};

class CSockInfo
{
public:
    curl_socket_t m_fdSock;
    // event *m_evpSock;
    aeFileEvent *m_evpSock;
    // event_base *m_evpBase;
    aeEventLoop *m_evpBase;
    string m_strIp;
    int m_iPort;
    int64_t m_i64TimeOut;
    int m_iTcpState = 0;   //tcp event专用状态 0-初始化，1-建立连接，2-write完成，3-read完成
};

void checkMultiInfo(CMultiInfo *const pMultiInfo)
{
    CURLMsg *pCurlMsg = NULL;
    int iMsgsInQueue = 0;
    CConnInfo *pConnInfo = NULL;

    while((pCurlMsg = curl_multi_info_read(pMultiInfo->m_curlm, (int*)&iMsgsInQueue)) != NULL)
    {
        Log("msg: %d, easy_handle: %p, result: %d", pCurlMsg->msg, pCurlMsg->easy_handle, pCurlMsg->data.result);
        if(pCurlMsg->msg == CURLMSG_DONE)
        {
            assert(curl_easy_getinfo(pCurlMsg->easy_handle, CURLINFO_PRIVATE, (void**)&pConnInfo) == CURLE_OK);
            assert(pCurlMsg->easy_handle == pConnInfo->m_curl);
            Log("url[%s] done, resp[%s]", pConnInfo->m_strUrl.c_str(), pConnInfo->m_strResp.c_str());

            if(false)    //assume clean all
            {
                // event_base_loopbreak(pMultiInfo->m_evpBase);
                aeStop(pMultiInfo->m_evpBase);
                return;
            }

            assert(curl_multi_remove_handle(pMultiInfo->m_curlm, pCurlMsg->easy_handle) == CURLM_OK);
            curl_easy_cleanup(pCurlMsg->easy_handle);
            delete pConnInfo;
        }
    }
    if(pMultiInfo->m_iRunning <= 0)
    {
        // event_base_loopbreak(pMultiInfo->m_evpBase);
        // Log("maxfd: %d", pMultiInfo->m_evpBase->maxfd);     // -1
        aeStop(pMultiInfo->m_evpBase);   //这里虽然所有事件都清空了，但是epoll_wait会一直阻塞，需要手动调用aeStop从循环里退出来
        //不过如果想multiTimerCallback branch 1中超时事件设置的回调函数触发的话，那么这行需要注释掉，才能看到效果。
    }
}

// void evSockCallback(evutil_socket_t fdTimeout, short sEvent, void *arg)
void evSockCallback(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
{
    Log("evSockCallback begin, eventLoop: %p, fd: %d, clientData: %p, mask: %d", eventLoop, fd, clientData, mask);

    CMultiInfo *pMultiInfo = (CMultiInfo*)clientData;
    int iEvBitMask = (mask & AE_READABLE ? CURL_CSELECT_IN : 0) | (mask & AE_WRITABLE ? CURL_CSELECT_OUT : 0);

    CURLMcode mcode = curl_multi_socket_action(pMultiInfo->m_curlm, fd, iEvBitMask, (int*)&pMultiInfo->m_iRunning);
    assert(mcode == CURLM_OK);

    checkMultiInfo(pMultiInfo);

    if(pMultiInfo->m_iRunning <= 0)
    {
        //...
    }
}

// void evTimerCallback(evutil_socket_t fdTimeout, short sEvent, void *arg)
void evTimerCallback(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
{
    Log("evTimerCallback begin, eventLoop: %p, fd: %d, clientData: %p, mask: %d", eventLoop, fd, clientData, mask);

    CMultiInfo *pMultiInfo = (CMultiInfo*)clientData;
    if(fd > 0)
    {
        uint64_t ulTimeoutNum = 0;
        ::read(fd, &ulTimeoutNum, sizeof(uint64_t));
        Log("ulTimeoutNum: %lu", ulTimeoutNum);
        aeDeleteFileEvent(pMultiInfo->m_evpBase, fd, AE_READABLE);
        close(fd);
        return;
    }
    // Log("curl_multi_socket_action begin, curlm: %p, running: %d", pMultiInfo->m_curlm, pMultiInfo->m_iRunning);
    CURLMcode mcode = curl_multi_socket_action(pMultiInfo->m_curlm, CURL_SOCKET_TIMEOUT, 0, &pMultiInfo->m_iRunning);
    // Log("curl_multi_socket_action finish, curlm: %p, running: %d", pMultiInfo->m_curlm, pMultiInfo->m_iRunning);
    assert(mcode == CURLM_OK);
}

int multiTimerCallback(CURLM *multi, long timeout_ms, void *userp)
{
    Log("multiTimerCallback begin, multi: %p, timeout_ms: %ld, userp: %p", multi, timeout_ms, userp);

    CMultiInfo *pMultiInfo = (CMultiInfo*)userp;

    if(timeout_ms > 0)      //branch 1
    {
        // timeval oTimeval{timeout_ms / 1000, timeout_ms % 1000 * 1000};
        // assert(evtimer_add(pMultiInfo->m_evpMultiTimer, &oTimeval) == 0);
        pMultiInfo->m_iMultiTimerFd = getTimer(timeout_ms / 1000, (uint64_t)timeout_ms % 1000 * 1000 * 1000);
        aeCreateFileEvent(pMultiInfo->m_evpBase, pMultiInfo->m_iMultiTimerFd, AE_READABLE, evTimerCallback, (void*)pMultiInfo);
    }
    else if(timeout_ms == 0)    //branch 2
    {
        // evTimerCallback(-1, EV_TIMEOUT, (void*)pMultiInfo);
        evTimerCallback(pMultiInfo->m_evpBase, -1, (void*)pMultiInfo, AE_READABLE);
    }
    else    //branch 3
    {
        //如果这里不删除的话那么branch 1中添加的m_iMultiTimerFd就会被触发
        // assert(evtimer_del(pMultiInfo->m_evpMultiTimer) == 0);
        aeDeleteFileEvent(pMultiInfo->m_evpBase, pMultiInfo->m_iMultiTimerFd, AE_READABLE);
    }
}

int multiSocketCallback(CURL *easy, curl_socket_t s, int what, void *userp, void *socketp)
{
    Log("multiSocketCallback begin, easy: %p, s: %d, what: %d, userp: %p, socketp: %p", easy, s, what, userp, socketp);

    CMultiInfo *pMultiInfo = (CMultiInfo*)userp;
    CSockInfo *pSockInfo = (CSockInfo*)socketp;

    if(what & CURL_POLL_REMOVE)
    {
        // assert(event_del(pSockInfo->m_evpSock) == 0);
        aeDeleteFileEvent(pMultiInfo->m_evpBase, s, AE_READABLE | AE_WRITABLE);
        delete pSockInfo;
    }
    else
    {
        // short sEvent = (what & CURL_POLL_IN ? EV_READ | EV_PERSIST : 0) | (what & CURL_POLL_OUT ? EV_WRITE | EV_PERSIST : 0);
        int iMask = (what & CURL_POLL_IN ? AE_READABLE : 0) | (what & CURL_POLL_OUT ? AE_WRITABLE : 0);
        if(pSockInfo == NULL)
        {
            pSockInfo = new CSockInfo;
            // pSockInfo->m_evpSock = event_new(pMultiInfo->m_evpBase, s, sEvent, evSockCallback, (void*)pMultiInfo);
            curl_multi_assign(pMultiInfo->m_curlm, s, (void*)pSockInfo);
        }
        else
        {
            assert(pSockInfo->m_evpSock != NULL);       // 这里竟然不报错？野指针！
            // assert(event_del(pSockInfo->m_evpSock) == 0);
            //此处一定要删干净，所有事件都要删干净，之前只删了iMask(上次注册的写事件没有删干净)，而curl_multi_socket_action不会重复
            //处理写事件的，因此导致evSockCallback因为写事件被不断触发而不断回调，造成了满屏幕的输出。
            aeDeleteFileEvent(pMultiInfo->m_evpBase, s, iMask | AE_READABLE | AE_WRITABLE);
            // event_assign(pSockInfo->m_evpSock, pMultiInfo->m_evpBase, s, sEvent, evSockCallback, (void*)pMultiInfo);
        }
        // assert(event_add(pSockInfo->m_evpSock, NULL) == 0);
        aeCreateFileEvent(pMultiInfo->m_evpBase, s, iMask, evSockCallback, (void*)pMultiInfo);
    }
}

size_t easyWriteCallback(char *buffer, size_t size, size_t nitems, void *outstream)
{
    CConnInfo *pConnInfo = (CConnInfo*)outstream;
    pConnInfo->m_strResp.append(buffer, size * nitems);
    return size * nitems;
}

extern "C" {
    void ExtLog(const char *chMsg)
    {
        puts(chMsg);
    }
}

int addHttpCondi(CURLM *curlm, const string &strUrl, const string &strHost = g_strHost, int64_t i64TimeOutMs = g_i64TimeOut)
{
    CConnInfo *pConnInfo = new CConnInfo;
    CURL *curl = curl_easy_init();
    assert(curl != NULL);
    Curl_easy_setopt(curl, CURLOPT_URL, strUrl.c_str());
    if(i64TimeOutMs > 0)
    {
        Curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, i64TimeOutMs);
    }
    curl_slist *header = NULL;
    header = curl_slist_append(header, string("Host: " + strHost).c_str());
    assert(header != NULL);
    Curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
    Curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, easyWriteCallback);
    Curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)pConnInfo);
    Curl_easy_setopt(curl, CURLOPT_PRIVATE, (void*)pConnInfo);

    pConnInfo->m_curl = curl;
    pConnInfo->m_strUrl = strUrl;

    // Log("timer_cb: %p", (Curl_multi*)curlm->timer_cb);
    CURLMcode mcode = curl_multi_add_handle(curlm, pConnInfo->m_curl);
    // Log("timer_cb: %p", (Curl_multi*)curlm->timer_cb);

    Log("curl_multi_add_handle finish, mcode: %d", mcode);
    return mcode == CURLM_OK ? 0 : -1;
}

// void tcpCallback(evutil_socket_t sockClient, short sEvent, void *arg)
void tcpCallback(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
{
    Log("tcpCallback begin, eventLoop: %p, fd: %d, clientData: %p, mask: %d", eventLoop, fd, clientData, mask);

    CSockInfo *pSockInfo = (CSockInfo*)clientData;

    if((mask & AE_WRITABLE) != 0 && pSockInfo->m_iTcpState == 0)
    {
        sockaddr_in sinServer;
        sinServer.sin_family = AF_INET;
        sinServer.sin_port = htons(pSockInfo->m_iPort);
        assert(inet_aton(pSockInfo->m_strIp.c_str(), &(sinServer.sin_addr)) != 0);

        int iRes = 23456;
        iRes = ::connect(fd, (sockaddr*)&sinServer, sizeof(sinServer));
        if(iRes != 0)
        {
            PerrorLog("connect", "fd: %d, iRes: %d, errno: %d", fd, iRes, errno);
            if(errno != EINPROGRESS)
            {
                Log("error....................");
                // event_free(pSockInfo->m_evpSock);
                aeDeleteFileEvent(pSockInfo->m_evpBase, fd, AE_READABLE | AE_WRITABLE);
                // assert(evutil_closesocket(sockClient) == 0);
                assert(::close(fd) == 0);
                return;
            }
        }
        pSockInfo->m_iTcpState = 1;
        // assert(event_del(pSockInfo->m_evpSock) == 0);
        // event_assign(pSockInfo->m_evpSock, pSockInfo->m_evpBase, sockClient, EV_WRITE, tcpCallback, (void*)pSockInfo);
        // timeval oTimeval{pSockInfo->m_i64TimeOut / 1000, pSockInfo->m_i64TimeOut % 1000 * 1000};
        // event_add(pSockInfo->m_evpSock, &oTimeval);
        return;
    }
    if((mask & AE_WRITABLE) != 0 && pSockInfo->m_iTcpState == 1)
    {
        char chSendBuf[1024] = "\0";
        sprintf(chSendBuf, "client[%d] send hello", fd);
        assert(::write(fd, chSendBuf, strlen(chSendBuf) + 1) >= 0);
        Log("client[%d] write finish", fd);
        pSockInfo->m_iTcpState = 2;
        // assert(event_del(pSockInfo->m_evpSock) == 0);
        // event_assign(pSockInfo->m_evpSock, pSockInfo->m_evpBase, sockClient, EV_READ, tcpCallback, (void*)pSockInfo);
        aeDeleteFileEvent(pSockInfo->m_evpBase, fd, AE_WRITABLE);
        aeCreateFileEvent(pSockInfo->m_evpBase, fd, AE_READABLE, tcpCallback, (void*)pSockInfo);
        // timeval oTimeval{pSockInfo->m_i64TimeOut / 1000, pSockInfo->m_i64TimeOut % 1000 * 1000};
        // event_add(pSockInfo->m_evpSock, &oTimeval);
        return;
    }
    if((mask & AE_READABLE) != 0 && pSockInfo->m_iTcpState == 2)
    {
        char chRecvBuf[1024] = "\0";
        int iRecvSize = ::read(fd, chRecvBuf, sizeof(chRecvBuf));
        Log("client[%d] read finish, iRecvSize: %d, chRecvBuf: %s", fd, iRecvSize, chRecvBuf);
        pSockInfo->m_iTcpState = 3;
        // event_free(pSockInfo->m_evpSock);
        aeDeleteFileEvent(pSockInfo->m_evpBase, fd, AE_READABLE | AE_WRITABLE);
        // assert(evutil_closesocket(sockClient) == 0);
        assert(::close(fd) == 0);

        if(pSockInfo->m_evpBase->maxfd <= 0 || aeGetSetSize(pSockInfo->m_evpBase) <= 0)
        {
            aeStop(pSockInfo->m_evpBase);
        }
        return;
    }
    Log("error...........................................................");
    // event_free(pSockInfo->m_evpSock);
    // assert(evutil_closesocket(sockClient) == 0);
    aeDeleteFileEvent(pSockInfo->m_evpBase, fd, AE_READABLE | AE_WRITABLE);
    assert(::close(fd) == 0);
}

// int addTcpCondi(event_base *evpBase, const string &strIp, int iPort, int64_t i64TimeOutMs = g_i64TimeOut)
int addTcpCondi(aeEventLoop *evpBase, const string &strIp, int iPort, int64_t i64TimeOutMs = g_i64TimeOut)
{
    CSockInfo *pSockInfo = new CSockInfo;
    pSockInfo->m_strIp = strIp;
    pSockInfo->m_iPort = iPort;
    pSockInfo->m_i64TimeOut = i64TimeOutMs;
    pSockInfo->m_evpBase = evpBase;

    int sockClient = ::socket(AF_INET, SOCK_STREAM, 0);
    if(sockClient < 0)
    {
        PerrorLog("socket", "sockClient: %d", sockClient);
        return -1;
    }
    // int iErrno = evutil_make_socket_nonblocking(sockClient);
    int iErrno = setNonBlock(sockClient);
    if(iErrno < 0)
    {
        Log("setNonBlock fail, ret: %d", iErrno);
        return -2;
    }
    // pSockInfo->m_evpSock = event_new(NULL, -1, 0, NULL, NULL);
    // assert(event_assign(pSockInfo->m_evpSock, evpBase, sockClient, EV_READ | EV_WRITE, tcpCallback, (void *)pSockInfo) == 0);
    aeCreateFileEvent(pSockInfo->m_evpBase, sockClient, AE_READABLE | AE_WRITABLE, tcpCallback, (void*)pSockInfo);
    // timeval oTimeval{i64TimeOutMs / 1000, i64TimeOutMs % 1000 * 1000};
    // assert(event_add(pSockInfo->m_evpSock, &oTimeval) == 0);

    return 0;
}

struct CNoIOInfo
{
    // event *m_evpNoIO;
    aeFileEvent *m_evpNoIO;
    timeval m_tv;
};

// void noIOCallback(evutil_socket_t sockClient, short sEvent, void *arg)
void noIOCallback(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
{
    Log("noIOCallback begin, eventLoop: %p, fd: %d, clientData: %p, mask: %d", eventLoop, fd, clientData, mask);

    CNoIOInfo *oNoIOInfo = (CNoIOInfo*)clientData;
    timeval tvNow;
    gettimeofday(&tvNow, NULL);
    timeval oTvLast = oNoIOInfo->m_tv;
    long long tDiff = (tvNow.tv_sec - oTvLast.tv_sec) * 1000000 + (tvNow.tv_usec - oTvLast.tv_usec);
    Log("noIOCallback finish, now: %ld s, %ld us, last_time: %ld s, %ld us, diff: %ld us", 
        tvNow.tv_sec, tvNow.tv_usec, oTvLast.tv_sec, oTvLast.tv_usec, tDiff);

    // evtimer_del(oNoIOInfo->m_evpNoIO);
    aeDeleteFileEvent(eventLoop, fd, AE_READABLE);
    delete oNoIOInfo;
}

int addNoIOCondi(aeEventLoop *evpBase)
{
    CNoIOInfo *oNoIOInfo = new CNoIOInfo;
    gettimeofday(&oNoIOInfo->m_tv, NULL);
    // oNoIOInfo->m_evpNoIO = new event;
    // assert(evtimer_assign(oNoIOInfo->m_evpNoIO, evpBase, noIOCallback, (void*)oNoIOInfo) == 0);
    // timeval oTimeval{0, 0};
    // assert(evtimer_add(oNoIOInfo->m_evpNoIO, &oTimeval) == 0);
    int iTimerfd = getTimer(0, 1L * 1 * 1);
    aeCreateFileEvent(evpBase, iTimerfd, AE_READABLE, (aeFileProc*)noIOCallback, (void*)&oNoIOInfo->m_tv);  
    return 0;
}

int main(int argc, char const *argv[])
{
    CMultiInfo oMultiInfo;
    oMultiInfo.m_curlm = curl_multi_init();
    Log("multiTimerCallback: %p", multiTimerCallback);
    Curl_multi_setopt(oMultiInfo.m_curlm, CURLMOPT_TIMERFUNCTION, multiTimerCallback);
    Curl_multi_setopt(oMultiInfo.m_curlm, CURLMOPT_TIMERDATA, &oMultiInfo);
    Curl_multi_setopt(oMultiInfo.m_curlm, CURLMOPT_SOCKETFUNCTION, multiSocketCallback);
    Curl_multi_setopt(oMultiInfo.m_curlm, CURLMOPT_SOCKETDATA, &oMultiInfo);
    Log("m_curlm init finish");

    // oMultiInfo.m_evpBase = event_base_new();
    oMultiInfo.m_evpBase = aeCreateEventLoop(1024);
    // oMultiInfo.m_evpMultiTimer = new event;
    // evtimer_assign(oMultiInfo.m_evpMultiTimer, oMultiInfo.m_evpBase, evTimerCallback, (void*)&oMultiInfo);
    
    Log("m_evpBase init finish");

    assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "100") == 0);
    // assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "200") == 0);
    // assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "300") == 0);
    // assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "200") == 0);
    // assert(addTcpCondi(oMultiInfo.m_evpBase, "127.0.0.1", 9006) == 0);
    // assert(addTcpCondi(oMultiInfo.m_evpBase, "127.0.0.1", 9006) == 0);
    // assert(addTcpCondi(oMultiInfo.m_evpBase, "127.0.0.1", 9006) == 0);
    // assert(addNoIOCondi(oMultiInfo.m_evpBase) == 0);
    // assert(addNoIOCondi(oMultiInfo.m_evpBase) == 0);

    // event_base_dispatch(oMultiInfo.m_evpBase);
    aeMain(oMultiInfo.m_evpBase);
    // aeProcessEvents(oMultiInfo.m_evpBase, AE_FILE_EVENTS | AE_CALL_AFTER_SLEEP);
    Log("aeMain finish");

    //clean up...
    //

    return 0;
}
