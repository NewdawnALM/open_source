#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "curl/curl.h"
#include "event.h"
#include "comdef.h"
#include "env_config.h"
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <assert.h>
using namespace std;

const int64_t g_i64TimeOut = 3900;  //ms

#define Curl_easy_setopt(curl, option, argv...) \
    assert(curl_easy_setopt(curl, option, ##argv) == CURLE_OK);

#define Curl_multi_setopt(curl, option, argv...) \
    assert(curl_multi_setopt(curl, option, ##argv) == CURLM_OK);


class CMultiInfo
{
public:
    CURLM *m_curlm;
    int m_iRunning;
    event_base *m_evpBase;
    event *m_evpMultiTimer;
    // event *m_evAddHandlerTimer;
};

class CConnInfo
{
public:
    string m_strUrl;
    string m_strHost;
    // int m_iMethod;
    // string m_strParam;
    int64_t m_i64TimeOut;
    CURL *m_curl;
    string m_strResp;
};

class CSockInfo
{
public:
    curl_socket_t m_fdSock;
    event *m_evpSock;
    event_base *m_evpBase;
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
                event_base_loopbreak(pMultiInfo->m_evpBase);
                return;
            }

            assert(curl_multi_remove_handle(pMultiInfo->m_curlm, pCurlMsg->easy_handle) == CURLM_OK);
            curl_easy_cleanup(pCurlMsg->easy_handle);
            delete pConnInfo;
        }
    }
    if(pMultiInfo->m_iRunning <= 0)
    {
        event_base_loopbreak(pMultiInfo->m_evpBase);
    }
}

void evSockCallback(evutil_socket_t fdTimeout, short sEvent, void *arg)
{
    Log("evSockCallback begin, fdTimeout: %d, sEvent: %d, arg: %p", fdTimeout, sEvent, arg);

    CMultiInfo *pMultiInfo = (CMultiInfo*)arg;
    int iEvBitMask = (sEvent & EV_READ ? CURL_CSELECT_IN : 0) | (sEvent & EV_WRITE ? CURL_CSELECT_OUT : 0);

    CURLMcode mcode = curl_multi_socket_action(pMultiInfo->m_curlm, fdTimeout, iEvBitMask, (int*)&pMultiInfo->m_iRunning);
    assert(mcode == CURLM_OK);

    checkMultiInfo(pMultiInfo);

    if(pMultiInfo->m_iRunning <= 0)
    {
        //...
    }
}

void evTimerCallback(evutil_socket_t fdTimeout, short sEvent, void *arg)
{
    Log("evTimerCallback begin, fdTimeout: %d, sEvent: %d, arg: %p", fdTimeout, sEvent, arg);
    CMultiInfo *pMultiInfo = (CMultiInfo*)arg;
    Log("curl_multi_socket_action begin, curlm: %p, running: %d", pMultiInfo->m_curlm, pMultiInfo->m_iRunning);
    CURLMcode mcode = curl_multi_socket_action(pMultiInfo->m_curlm, CURL_SOCKET_TIMEOUT, 0, &pMultiInfo->m_iRunning);
    Log("curl_multi_socket_action finish, curlm: %p, running: %d", pMultiInfo->m_curlm, pMultiInfo->m_iRunning);
    assert(mcode == CURLM_OK);
}

int multiTimerCallback(CURLM *multi, long timeout_ms, void *userp)
{
    Log("multiTimerCallback begin, multi: %p, timeout_ms: %ld, userp: %p", multi, timeout_ms, userp);

    CMultiInfo *pMultiInfo = (CMultiInfo*)userp;

    if(timeout_ms > 0)
    {
        timeval oTimeval{timeout_ms / 1000, timeout_ms % 1000 * 1000};
        assert(evtimer_add(pMultiInfo->m_evpMultiTimer, &oTimeval) == 0);
    }
    else if(timeout_ms == 0)
    {
        evTimerCallback(-1, EV_TIMEOUT, (void*)pMultiInfo);
    }
    else
    {
        assert(evtimer_del(pMultiInfo->m_evpMultiTimer) == 0);
    }
}

int multiSocketCallback(CURL *easy, curl_socket_t s, int what, void *userp, void *socketp)
{
    Log("multiSocketCallback begin, easy: %p, s: %d, what: %d, userp: %p, socketp: %p", easy, s, what, userp, socketp);

    CMultiInfo *pMultiInfo = (CMultiInfo*)userp;
    CSockInfo *pSockInfo = (CSockInfo*)socketp;

    if(what & CURL_POLL_REMOVE)
    {
        assert(event_del(pSockInfo->m_evpSock) == 0);
        delete pSockInfo;
    }
    else
    {
        short sEvent = (what & CURL_POLL_IN ? EV_READ | EV_PERSIST : 0) | (what & CURL_POLL_OUT ? EV_WRITE | EV_PERSIST : 0);
        if(pSockInfo == NULL)
        {
            pSockInfo = new CSockInfo;
            pSockInfo->m_evpSock = event_new(pMultiInfo->m_evpBase, s, sEvent, evSockCallback, (void*)pMultiInfo);
            curl_multi_assign(pMultiInfo->m_curlm, s, (void*)pSockInfo);
        }
        else
        {
            assert(pSockInfo->m_evpSock != NULL);
            assert(event_del(pSockInfo->m_evpSock) == 0);
            event_assign(pSockInfo->m_evpSock, pMultiInfo->m_evpBase, s, sEvent, evSockCallback, (void*)pMultiInfo);
        }
        assert(event_add(pSockInfo->m_evpSock, NULL) == 0);
    }
}

size_t easyWriteCallback(char *buffer, size_t size, size_t nitems, void *outstream)
{
    CConnInfo *pConnInfo = (CConnInfo*)outstream;
    pConnInfo->m_strResp.append(buffer, size * nitems);
    return size * nitems;
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
    CURLMcode mcode = curl_multi_add_handle(curlm, pConnInfo->m_curl);
    return mcode == CURLM_OK ? 0 : -1;
}

void tcpCallback(evutil_socket_t sockClient, short sEvent, void *arg)
{
    Log("tcpCallback begin, sockClient: %d, sEvent: %d, arg: %p", sockClient, sEvent, arg);

    CSockInfo *pSockInfo = (CSockInfo*)arg;

    if((sEvent & EV_WRITE) != 0 && pSockInfo->m_iTcpState == 0)
    {
        sockaddr_in sinServer;
        sinServer.sin_family = AF_INET;
        sinServer.sin_port = htons(pSockInfo->m_iPort);
        assert(inet_aton(pSockInfo->m_strIp.c_str(), &(sinServer.sin_addr)) != 0);

        int iRes = 23456;
        iRes = ::connect(sockClient, (sockaddr*)&sinServer, sizeof(sinServer));
        if(iRes != 0)
        {
            PerrorLog("connect", "sockClient: %d, iRes: %d, errno: %d", sockClient, iRes, errno);
            if(errno != EINPROGRESS)
            {
                Log("error....................");
                event_free(pSockInfo->m_evpSock);
                assert(evutil_closesocket(sockClient) == 0);
                return;
            }
        }
        pSockInfo->m_iTcpState = 1;
        assert(event_del(pSockInfo->m_evpSock) == 0);
        event_assign(pSockInfo->m_evpSock, pSockInfo->m_evpBase, sockClient, EV_WRITE, tcpCallback, (void*)pSockInfo);
        timeval oTimeval{pSockInfo->m_i64TimeOut / 1000, pSockInfo->m_i64TimeOut % 1000 * 1000};
        event_add(pSockInfo->m_evpSock, &oTimeval);
        return;
    }
    if((sEvent & EV_WRITE) != 0 && pSockInfo->m_iTcpState == 1)
    {
        char chSendBuf[1024] = "\0";
        sprintf(chSendBuf, "client[%d] send hello", sockClient);
        assert(::write(sockClient, chSendBuf, strlen(chSendBuf) + 1) >= 0);
        Log("client[%d] write finish", sockClient);
        pSockInfo->m_iTcpState = 2;
        assert(event_del(pSockInfo->m_evpSock) == 0);
        event_assign(pSockInfo->m_evpSock, pSockInfo->m_evpBase, sockClient, EV_READ, tcpCallback, (void*)pSockInfo);
        timeval oTimeval{pSockInfo->m_i64TimeOut / 1000, pSockInfo->m_i64TimeOut % 1000 * 1000};
        event_add(pSockInfo->m_evpSock, &oTimeval);
        return;
    }
    if((sEvent & EV_READ) != 0 && pSockInfo->m_iTcpState == 2)
    {
        char chRecvBuf[1024] = "\0";
        int iRecvSize = ::read(sockClient, chRecvBuf, sizeof(chRecvBuf));
        Log("client[%d] read finish, iRecvSize: %d, chRecvBuf: %s", sockClient, iRecvSize, chRecvBuf);
        pSockInfo->m_iTcpState = 3;
        event_free(pSockInfo->m_evpSock);
        assert(evutil_closesocket(sockClient) == 0);
        return;
    }
    Log("error...........................................................");
    event_free(pSockInfo->m_evpSock);
    assert(evutil_closesocket(sockClient) == 0);
}

int addTcpCondi(event_base *evpBase, const string &strIp, int iPort, int64_t i64TimeOutMs = g_i64TimeOut)
{
    CSockInfo *pSockInfo = new CSockInfo;
    pSockInfo->m_strIp = strIp;
    pSockInfo->m_iPort = iPort;
    pSockInfo->m_i64TimeOut = i64TimeOutMs;
    pSockInfo->m_evpBase = evpBase;

    evutil_socket_t sockClient = ::socket(AF_INET, SOCK_STREAM, 0);
    if(sockClient < 0)
    {
        PerrorLog("socket", "sockClient: %d", sockClient);
        return -1;
    }
    int iErrno = evutil_make_socket_nonblocking(sockClient);
    if(iErrno < 0)
    {
        Log("make_socket_nonblocking fail, ret: %d", iErrno);
        return -2;
    }
    pSockInfo->m_evpSock = event_new(NULL, -1, 0, NULL, NULL);
    assert(event_assign(pSockInfo->m_evpSock, evpBase, sockClient, EV_READ | EV_WRITE, tcpCallback, (void *)pSockInfo) == 0);
    timeval oTimeval{i64TimeOutMs / 1000, i64TimeOutMs % 1000 * 1000};
    assert(event_add(pSockInfo->m_evpSock, &oTimeval) == 0);

    return 0;
}

struct CNoIOInfo
{
    event *m_evpNoIO;
    timeval m_tv;
};

void noIOCallback(evutil_socket_t sockClient, short sEvent, void *arg)
{
    Log("noIOCallback begin, sockClient: %d, sEvent: %d, arg: %p", sockClient, sEvent, arg);

    CNoIOInfo *oNoIOInfo = (CNoIOInfo*)arg;
    timeval tvNow;
    gettimeofday(&tvNow, NULL);
    timeval oTvLast = oNoIOInfo->m_tv;
    long long tDiff = (tvNow.tv_sec - oTvLast.tv_sec) * 1000000 + (tvNow.tv_usec - oTvLast.tv_usec);
    Log("noIOCallback finish, now: %ld s, %ld us, last_time: %ld s, %ld us, diff: %ld us", 
        tvNow.tv_sec, tvNow.tv_usec, oTvLast.tv_sec, oTvLast.tv_usec, tDiff);

    evtimer_del(oNoIOInfo->m_evpNoIO);
    delete oNoIOInfo;
}

int addNoIOCondi(event_base *evpBase)
{
    CNoIOInfo *oNoIOInfo = new CNoIOInfo;
    gettimeofday(&oNoIOInfo->m_tv, NULL);
    oNoIOInfo->m_evpNoIO = new event;
    assert(evtimer_assign(oNoIOInfo->m_evpNoIO, evpBase, noIOCallback, (void*)oNoIOInfo) == 0);
    timeval oTimeval{0, 0};
    assert(evtimer_add(oNoIOInfo->m_evpNoIO, &oTimeval) == 0);
    return 0;
}

int main(int argc, char const *argv[])
{
    CMultiInfo oMultiInfo;
    oMultiInfo.m_curlm = curl_multi_init();
    Curl_multi_setopt(oMultiInfo.m_curlm, CURLMOPT_TIMERFUNCTION, multiTimerCallback);
    Curl_multi_setopt(oMultiInfo.m_curlm, CURLMOPT_TIMERDATA, &oMultiInfo);
    Curl_multi_setopt(oMultiInfo.m_curlm, CURLMOPT_SOCKETFUNCTION, multiSocketCallback);
    Curl_multi_setopt(oMultiInfo.m_curlm, CURLMOPT_SOCKETDATA, &oMultiInfo);
    Log("m_curlm init finish");

    oMultiInfo.m_evpBase = event_base_new();
    oMultiInfo.m_evpMultiTimer = new event;
    evtimer_assign(oMultiInfo.m_evpMultiTimer, oMultiInfo.m_evpBase, evTimerCallback, (void*)&oMultiInfo);
    Log("m_evpBase init finish");

    assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "100") == 0);
    // assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "200") == 0);
    // assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "300") == 0);
    // assert(addHttpCondi(oMultiInfo.m_curlm, g_strUrlPrefix + "200") == 0);
    // assert(addTcpCondi(oMultiInfo.m_evpBase, "127.0.0.1", 9006) == 0);
    // assert(addTcpCondi(oMultiInfo.m_evpBase, "127.0.0.1", 9006) == 0);
    // assert(addTcpCondi(oMultiInfo.m_evpBase, "127.0.0.1", 9006) == 0);
    // assert(addTcpCondi(oMultiInfo.m_evpBase, "127.0.0.1", 9006) == 0);
    // assert(addTcpCondi(oMultiInfo.m_evpBase, "127.0.0.1", 9006) == 0);
    // assert(addNoIOCondi(oMultiInfo.m_evpBase) == 0);
    // assert(addNoIOCondi(oMultiInfo.m_evpBase) == 0);
    // assert(addNoIOCondi(oMultiInfo.m_evpBase) == 0);
    // assert(addNoIOCondi(oMultiInfo.m_evpBase) == 0);
    // assert(addNoIOCondi(oMultiInfo.m_evpBase) == 0);
    // assert(addNoIOCondi(oMultiInfo.m_evpBase) == 0);

    event_base_dispatch(oMultiInfo.m_evpBase);
    Log("event_base_dispatch finish");

    //clean up...
    //

    return 0;
}
