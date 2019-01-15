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
#include "timeval.h"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>
#include <assert.h>
using namespace std;

const int64_t g_i64TimeOut = 4000;  //ms

#define Curl_easy_setopt(curl, option, argv...) \
    assert(curl_easy_setopt(curl, option, ##argv) == CURLE_OK);

#define Curl_multi_setopt(curl, option, argv...) \
    assert(curl_multi_setopt(curl, option, ##argv) == CURLM_OK);


int getTimer(int64_t i64Second, int64_t i64NSec, int iTimerfd = -1)
{
    if(iTimerfd < 0)
    {
        iTimerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    }
    if(iTimerfd < 0)
    {
        Log("timerfd_create error[%d], iTimerfd: %d", errno, iTimerfd);
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
        Log("timerfd_settime error[%d], iRes: %d", errno, iRes);
        return iRes;
    }
    return iTimerfd;
}


class CMultiInfo    //CDispatch
{
public:
    CURLM *m_curlm;
    int m_iRunning;
    aeEventLoop *m_evpBase;
    // event_base *m_evpBase;
    // event *m_evpMultiTimer;
    // int m_iMultiTimerFd;
    map<long, set<int> > m_mapTimeoutFds;
};

class CConnInfo     //CCondHttp
{
public:
    string m_strUrl;
    string m_strHost;
    int64_t m_i64TimeOut;   //没有使用
    // CURL *m_curl;
    map<int64_t, CURL*> s_mapCurl;
    // static map<int64_t, CURL*> s_mapCurl;
    curl_slist *m_header;
    // int m_iMultiTimerFd;    //每个http独有一个超时的fd
    string m_strResp;

    CConnInfo(): /*m_curl(NULL),*/ m_header(NULL) {}
    ~CConnInfo()
    {
        if(m_header != NULL)
        {
            curl_slist_free_all(m_header);
            m_header = NULL;
        }
        // if(m_curl != NULL)
        // {
        //     curl_easy_cleanup(m_curl);
        //     m_curl = NULL;
        // }
        CURL *curl = GetCurlHandle();
        if(curl != NULL)
        {
            // curl_easy_reset(curl);
            curl_easy_cleanup(curl);
            Log("~CConnInfo(), curl_easy_cleanup[%ld]", m_i64TimeOut);
        }
    }

    CURL* GetCurlHandle() const
    {
        map<int64_t, CURL*>::const_iterator mit = s_mapCurl.find(m_i64TimeOut);
        return mit == s_mapCurl.end() ? NULL : mit->second;
    }
};

// map<int64_t, CURL*> CConnInfo::s_mapCurl;

enum E_TCP_STATE_DEF
{
    E_TCP_STATE_INIT = 0,
    E_TCP_STATE_CONNECT = 1,
    E_TCP_STATE_WROTE = 2,
    E_TCP_STATE_READ = 3,
};

class CSockInfo     //CCondTcp
{
public:
    // curl_socket_t m_fdSock;
    // event *m_evpSock;
    aeFileEvent *m_evpSock;     //没有使用
    // event_base *m_evpBase;
    aeEventLoop *m_evpBase;     //没有使用
    string m_strIp;
    int m_iPort;
    int64_t m_i64TimeOut;   //没有使用
    int m_iTcpState;   //tcp event专用状态 0-初始化，1-建立连接，2-write完成，3-read完成
    int m_iFd;    //tcp连接fd
    long long m_lTimerId;   //定时器ID
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
            assert(pCurlMsg->easy_handle == pConnInfo->GetCurlHandle());
            Log("url[%s] done, resp[%s]", pConnInfo->m_strUrl.c_str(), pConnInfo->m_strResp.c_str());

            if(false)    //assume clean all
            {
                // event_base_loopbreak(pMultiInfo->m_evpBase);
                // aeStop(pMultiInfo->m_evpBase);
                return;
            }

            assert(curl_multi_remove_handle(pMultiInfo->m_curlm, pCurlMsg->easy_handle) == CURLM_OK);
            // curl_easy_cleanup(pCurlMsg->easy_handle);
            delete pConnInfo;
            pConnInfo = NULL;
        }
    }
    if(pMultiInfo->m_iRunning <= 0)
    {
        // event_base_loopbreak(pMultiInfo->m_evpBase);
        // Log("maxfd: %d", pMultiInfo->m_evpBase->maxfd);     // -1
        // aeStop(pMultiInfo->m_evpBase);   //这里虽然所有事件都清空了，但是epoll_wait会一直阻塞，需要手动调用aeStop从循环里退出来
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
}

// void evHttpTimerCb(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
int evHttpTimerCb(struct aeEventLoop *eventLoop, long long id, void *clientData)
{
    Log("evHttpTimerCb begin, eventLoop: %p, id: %d, clientData: %p", eventLoop, id, clientData);

    CMultiInfo *pMultiInfo = (CMultiInfo*)clientData;
    CURLMcode mcode = curl_multi_socket_action(pMultiInfo->m_curlm, CURL_SOCKET_TIMEOUT, 0, &pMultiInfo->m_iRunning);
    assert(mcode == CURLM_OK);
    return AE_NOMORE;
}

int multiTimerCallback(CURLM *multi, long timeout_ms, void *userp)
{
    Log("multiTimerCallback begin, multi: %p, timeout_ms: %ld, userp: %p", multi, timeout_ms, userp);

    CMultiInfo *pMultiInfo = (CMultiInfo*)userp;

    if(timeout_ms > 0)      //branch 1
    {
        // int iTimerfd = getTimer(timeout_ms / 1000, (uint64_t)timeout_ms % 1000 * 1000 * 1000);
        int iTimerfd = aeCreateTimeEvent(pMultiInfo->m_evpBase, timeout_ms, evHttpTimerCb, (void*)pMultiInfo, NULL);
        // pMultiInfo->m_mapTimeoutFds[timeout_ms].insert(iTimerfd);
        // aeCreateFileEvent(pMultiInfo->m_evpBase, iTimerfd, AE_READABLE, evTimerCallback, (void*)pMultiInfo);
    }
    else if(timeout_ms == 0)    //branch 2
    {
        evHttpTimerCb(pMultiInfo->m_evpBase, -1, (void*)pMultiInfo);
        // evTimerCallback(pMultiInfo->m_evpBase, -1, (void*)pMultiInfo, AE_READABLE);
    }
    else    //branch 3
    {
        //如果这里不删除的话那么branch 1中添加的m_iMultiTimerFd就会被触发
        // map<long, set<int> >::const_iterator mit_1 = pMultiInfo->m_mapTimeoutFds.begin();

        // map<long, set<int> >::const_iterator mit = pMultiInfo->m_mapTimeoutFds.find(timeout_ms);
        // if(mit == pMultiInfo->m_mapTimeoutFds.end())
        // {
        //     return 0;
        // }
        // if(mit->second.empty())
        // {
        //     pMultiInfo->m_mapTimeoutFds.erase(timeout_ms);
        //     return 0;
        // }
        // int iTimerfd = *(mit->second.begin());
        // assert(aeDeleteTimeEvent(pMultiInfo->m_evpBase, (long long)iTimerfd) == AE_OK);
        // // aeDeleteFileEvent(pMultiInfo->m_evpBase, iTimerfd, AE_READABLE);
        // // ::close(iTimerfd);   //这里如果不close的话会导致branch 1创建的fd泄露
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
        // Log("pMultiInfo->m_evpBase： %p", pMultiInfo->m_evpBase);
        aeDeleteFileEvent(pMultiInfo->m_evpBase, s, AE_READABLE | AE_WRITABLE);
        // Log("pMultiInfo->m_evpBase： %p", pMultiInfo->m_evpBase);
        delete pSockInfo;
        pSockInfo = NULL;
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
            // assert(pSockInfo->m_evpSock != NULL);
            // assert(event_del(pSockInfo->m_evpSock) == 0);
            //此处一定要删干净，所有事件都要删干净，之前只删了iMask(上次注册的写事件没有删干净)，而curl_multi_socket_action不会重复
            //处理写事件的，因此导致evSockCallback因为写事件被不断触发而不断回调，造成了满屏幕的输出。
            aeDeleteFileEvent(pMultiInfo->m_evpBase, s, iMask | AE_READABLE | AE_WRITABLE);
            // event_assign(pSockInfo->m_evpSock, pMultiInfo->m_evpBase, s, sEvent, evSockCallback, (void*)pMultiInfo);
        }
        // assert(event_add(pSockInfo->m_evpSock, NULL) == 0);
        aeCreateFileEvent(pMultiInfo->m_evpBase, s, iMask, evSockCallback, (void*)pMultiInfo);
    }
    return 0;
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
    // CURL *curl = curl_easy_init();
    // assert(curl != NULL);
    pConnInfo->m_i64TimeOut = i64TimeOutMs;
    CURL *curl = pConnInfo->GetCurlHandle();
    if(curl == NULL)
    {
        curl = curl_easy_init();
        assert(curl != NULL);
        Log("curl_easy_init[%ld]", i64TimeOutMs);
    }
    else
    {
        Log("Init(), curl_easy_reset[%ld]", i64TimeOutMs);
        curl_easy_reset(curl);
    }
    pConnInfo->s_mapCurl[i64TimeOutMs] = curl;

    Curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    Curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
    Curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 30L);

    Curl_easy_setopt(curl, CURLOPT_URL, strUrl.c_str());
    if(i64TimeOutMs > 0)
    {
        Curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, i64TimeOutMs);
    }
    pConnInfo->m_header = NULL;
    if(strHost != "")
    {
        pConnInfo->m_header = curl_slist_append(pConnInfo->m_header, string("Host: " + strHost).c_str());
        assert(pConnInfo->m_header != NULL);
        Curl_easy_setopt(curl, CURLOPT_HTTPHEADER, pConnInfo->m_header);
    }
    Curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, easyWriteCallback);
    Curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)pConnInfo);
    Curl_easy_setopt(curl, CURLOPT_PRIVATE, (void*)pConnInfo);

    // pConnInfo->m_curl = curl;
    pConnInfo->m_strUrl = strUrl;

    // Log("timer_cb: %p", (Curl_multi*)curlm->timer_cb);
    CURLMcode mcode = curl_multi_add_handle(curlm, curl);
    // Log("timer_cb: %p", (Curl_multi*)curlm->timer_cb);

    Log("curl_multi_add_handle[%ld] finish, mcode: %d", i64TimeOutMs, mcode);
    return mcode == CURLM_OK ? 0 : -1;
}

void beforeSleepProc(struct aeEventLoop *eventLoop)
{
    Log("beforeSleepProc begin, maxfd: %d, timeEventNextId: %d", eventLoop->maxfd, eventLoop->timeEventNextId);
    if(eventLoop->maxfd < 0)
    {
        // if(eventLoop->timeEventHead == NULL || eventLoop->timeEventHead->id == 0)
        // {
            Log("here stop");
            aeStop(eventLoop);
        // }
    }
}

int evTimerCb(struct aeEventLoop *eventLoop, long long id, void *clientData)
{
    Log("evTimerCb begin, eventLoop: %p, id: %d, clientData: %p", eventLoop, id, clientData);

    if(id == 0)
    {
        Log("event loop timeout reach, exit all");
        aeStop(eventLoop);
    }
    else
    {
        if(clientData != NULL)
        {
            CSockInfo *pCondTcp = (CSockInfo*)clientData;
            Log("tcp client timeout, id[%ld]", id);
            aeDeleteFileEvent(eventLoop, pCondTcp->m_iFd, AE_READABLE | AE_WRITABLE);
        }
    }
    return AE_NOMORE;
}

void evTcpReadCb(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
{
    Log("evTcpReadCb begin, eventLoop: %p, fd: %d, clientData: %p, mask: %d", eventLoop, fd, clientData, mask);
    CSockInfo *pCondTcp = (CSockInfo*)clientData;

    if((mask & AE_READABLE) == 0 || pCondTcp->m_iTcpState != E_TCP_STATE_WROTE)
    {
        Log("not tcp read event, abort");
        aeDeleteFileEvent(eventLoop, fd, AE_READABLE | AE_WRITABLE);
        assert(::close(fd) == 0);
        return;
    }
    char chRecvBuf[2048] = "\0";
    int iRecvSize = ::read(fd, chRecvBuf, sizeof(chRecvBuf));
    chRecvBuf[iRecvSize] = '\0';
    Log("client[%d] read finish, iRecvSize: %d, chRecvBuf: %s", fd, iRecvSize, chRecvBuf);

    // pCondTcp->SetResult(string(chRecvBuf));
    // pCondTcp->CheckResult();
    pCondTcp->m_iTcpState = E_TCP_STATE_READ;
    aeDeleteFileEvent(eventLoop, fd, AE_READABLE | AE_WRITABLE);
    aeDeleteTimeEvent(eventLoop, pCondTcp->m_lTimerId);
    assert(::close(fd) == 0);
    // delete pCondTcp;
}

void evTcpWriteCb(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
{
    Log("evTcpWriteCb begin, eventLoop: %p, fd: %d, clientData: %p, mask: %d", eventLoop, fd, clientData, mask);
    CSockInfo *pCondTcp = (CSockInfo*)clientData;
    
    if((mask & AE_WRITABLE) == 0 || pCondTcp->m_iTcpState != E_TCP_STATE_CONNECT)
    {
        Log("not tcp write event, abort");
        aeDeleteFileEvent(eventLoop, fd, AE_READABLE | AE_WRITABLE);
        assert(::close(fd) == 0);
        return;
    }
    char chSendBuf[1024];
    sprintf(chSendBuf, "client[%d] send hello", fd);
    assert(::write(fd, chSendBuf, strlen(chSendBuf) + 1) >= 0);
    Log("client[%d] write finish", fd);

    pCondTcp->m_iTcpState = E_TCP_STATE_WROTE;
    aeDeleteFileEvent(eventLoop, fd, AE_WRITABLE);      //更改为监听读事件
    aeCreateFileEvent(eventLoop, fd, AE_READABLE, evTcpReadCb, (void*)pCondTcp);
    pCondTcp->m_lTimerId = aeCreateTimeEvent(eventLoop, 1100, evTimerCb, (void*)pCondTcp, NULL);   //只等待1100ms
}

void evTcpConnectCb(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask)
{
    Log("evTcpConnectCb begin, eventLoop: %p, fd: %d, clientData: %p, mask: %d", eventLoop, fd, clientData, mask);
    CSockInfo *pCondTcp = (CSockInfo*)clientData;

    if((mask & AE_WRITABLE) == 0 || pCondTcp->m_iTcpState != E_TCP_STATE_INIT)
    {
        Log("not tcp connect event, abort");
        aeDeleteFileEvent(eventLoop, fd, AE_READABLE | AE_WRITABLE);
        assert(::close(fd) == 0);
        return;
    }
    sockaddr_in sinServer;
    sinServer.sin_family = AF_INET;
    sinServer.sin_port = htons(pCondTcp->m_iPort);
    assert(inet_aton(pCondTcp->m_strIp.c_str(), &(sinServer.sin_addr)) != 0);

    int iRes = ::connect(fd, (sockaddr*)&sinServer, sizeof(sinServer));
    if(iRes != 0)
    {
        if(errno != EINPROGRESS)
        {
            Log("connect fail, fd: %d, iRes: %d, errno: %d", fd, iRes, errno);
            aeDeleteFileEvent(eventLoop, fd, AE_READABLE | AE_WRITABLE);
            assert(::close(fd) == 0);
            return;
        }
    }
    pCondTcp->m_iTcpState = E_TCP_STATE_CONNECT;
    aeCreateFileEvent(eventLoop, fd, AE_WRITABLE, evTcpWriteCb, (void*)pCondTcp);
}

// int addTcpCondi(event_base *evpBase, const string &strIp, int iPort, int64_t i64TimeOutMs = g_i64TimeOut)
int addTcpCondi(aeEventLoop *evpBase, const string &strIp, int iPort, int64_t i64TimeOutMs = g_i64TimeOut)
{
    CSockInfo *pSockInfo = new CSockInfo;
    pSockInfo->m_strIp = strIp;
    pSockInfo->m_iPort = iPort;
    pSockInfo->m_i64TimeOut = i64TimeOutMs;
    pSockInfo->m_evpBase = evpBase;

    pSockInfo->m_iFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(pSockInfo->m_iFd < 0)
    {
        PerrorLog("socket", "sockClient: %d", pSockInfo->m_iFd);
        return -1;
    }
    // int iErrno = evutil_make_socket_nonblocking(pSockInfo->m_iFd);
    int iErrno = setNonBlock(pSockInfo->m_iFd);
    if(iErrno < 0)
    {
        Log("setNonBlock fail, ret: %d", iErrno);
        return -2;
    }
    // pSockInfo->m_evpSock = event_new(NULL, -1, 0, NULL, NULL);
    // assert(event_assign(pSockInfo->m_evpSock, evpBase, pSockInfo->m_iFd, EV_READ | EV_WRITE, tcpCallback, (void *)pSockInfo) == 0);
    aeCreateFileEvent(pSockInfo->m_evpBase, pSockInfo->m_iFd, AE_READABLE | AE_WRITABLE, evTcpConnectCb, (void*)pSockInfo);
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
    int iRetryTimes = 0;
loop:

    CTimeVal oTimer;

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
    aeCreateTimeEvent(oMultiInfo.m_evpBase, iRetryTimes & 1 ? 400 : 600, evTimerCb, NULL, NULL);
    aeSetBeforeSleepProc(oMultiInfo.m_evpBase, beforeSleepProc);
    // oMultiInfo.m_evpMultiTimer = new event;
    // evtimer_assign(oMultiInfo.m_evpMultiTimer, oMultiInfo.m_evpBase, evTimerCallback, (void*)&oMultiInfo);
    
    Log("m_evpBase init finish");

    assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "500", g_strHost, 1000) == 0);
    assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "300", g_strHost, 600) == 0);
    // assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "300") == 0);
    // assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "200") == 0);
    // assert(addTcpCondi(oMultiInfo.m_evpBase, "10.123.2.27", 9006) == 0);
    // assert(addTcpCondi(oMultiInfo.m_evpBase, "127.0.0.1", 9006) == 0);
    // assert(addTcpCondi(oMultiInfo.m_evpBase, "127.0.0.1", 9006) == 0);
    // assert(addNoIOCondi(oMultiInfo.m_evpBase) == 0);
    // assert(addNoIOCondi(oMultiInfo.m_evpBase) == 0);

    // event_base_dispatch(oMultiInfo.m_evpBase);
    aeMain(oMultiInfo.m_evpBase);
    // aeProcessEvents(oMultiInfo.m_evpBase, AE_FILE_EVENTS | AE_CALL_AFTER_SLEEP);

    curl_multi_cleanup(oMultiInfo.m_curlm);
    oMultiInfo.m_curlm = NULL;
    
    for(map<long, set<int> >::const_iterator mit = oMultiInfo.m_mapTimeoutFds.begin(); 
        mit != oMultiInfo.m_mapTimeoutFds.end(); ++mit)
    {
        for(set<int>::const_iterator sit = mit->second.begin(); sit != mit->second.end(); ++sit)
        {
            ::close(*sit);
        }
    }

    aeDeleteEventLoop(oMultiInfo.m_evpBase);    //必须最后在析构，否则由curl_multi_cleanup触发的multiSockCb调用会导致core
    oMultiInfo.m_evpBase = NULL;
    Log("aeMain finish, cost_time: %ld ms", oTimer.CostTime());

    ++iRetryTimes;

    int x;
    if(cin >> x)
    {
        if(x > 0)   goto loop;
    }
    return 0;
}
