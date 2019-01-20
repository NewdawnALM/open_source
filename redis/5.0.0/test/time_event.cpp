#ifdef  USE_AE3
    #include "ae3.h"
#else
    #ifdef  USE_AE2
        #include "ae2.h"
    #else
        #include "ae.h"
    #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
// #include <sys/time.h>
// #include <sys/timerfd.h>
#include <assert.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <queue>
#include <set>
using namespace std;
#include "comdef.h"
#include "timeval.h"

int g_iTimerNum = 100000;

#define  _assert(bExpression) \
{ \
    assert(bExpression); \
}


int evTimeProc_3(struct aeEventLoop *eventLoop, const aeTimeEvent *te)
{
    // Log("evTimeProc begin, id: %lld", id);

    if(te->clientData != NULL)
    {
        CTimeVal *pTimeVal = (CTimeVal*)(te->clientData);
        // Log("timer[%lld] cost time: %lld ms", te->id, pTimeVal->CostTime());
        delete pTimeVal;
    }
    // if(false && rand() & 1)
    if(rand() & 1)
    {
        static set<int> setDelId;
        setDelId.insert(te->id);

        int iDelId = rand() % g_iTimerNum;
        if(setDelId.find(iDelId) == setDelId.end())
        {
            #ifdef  USE_AE3
                aeDeleteTimeEvent(eventLoop, te);
            #endif
            setDelId.insert(iDelId);
            // Log("del id: %d", iDelId);
        }
    }
    return AE_NOMORE;
}

int evTimeProc(struct aeEventLoop *eventLoop, long long id, void *clientData)
{
    // Log("evTimeProc begin, id: %lld", id);

    if(clientData != NULL)
    {
        CTimeVal *pTimeVal = (CTimeVal*)clientData;
        // Log("timer[%lld] cost time: %lld ms", id, pTimeVal->CostTime());
        delete pTimeVal;
    }
    // if(false && rand() & 1)
    if(rand() & 1)
    {
        static set<int> setDelId;
        setDelId.insert(id);

        int iDelId = rand() % g_iTimerNum;
        if(setDelId.find(iDelId) == setDelId.end())
        {
            #ifndef USE_AE3
                aeDeleteTimeEvent(eventLoop, iDelId);
            #endif
            setDelId.insert(iDelId);
            // Log("del id: %d", iDelId);
        }
    }
    return AE_NOMORE;
}


void beforeSleepProc(struct aeEventLoop *eventLoop)
{
    // Log("beforeSleepProc begin, maxfd: %d, timeEventNextId: %lld", eventLoop->maxfd, eventLoop->timeEventNextId);

    if(eventLoop->maxfd < 0)
    {
        #ifdef  USE_AE3
            if(eventLoop->timeEventMap->size() == 0)
        #else
            #ifdef  USE_AE2
                if(eventLoop->timeMinHead->size() == 0)
            #else
                if(eventLoop->timeEventHead == NULL)
            #endif
        #endif
        {
            #ifndef  USE_AE3
                Log("here stop");
            #endif
            aeStop(eventLoop);
        }
    }
}

int main(int argc, char const *argv[])
{
    CTimeVal oMainTimer;

    if(argc < 2)
    {
        Log("usage: %s [timer_num]", argv[0]);
        exit(1);
    }
    g_iTimerNum = atoi(argv[1]);

    srand(time(NULL));

    aeEventLoop *eventLoop = aeCreateEventLoop(1024);
    _assert(eventLoop != NULL);
    aeSetBeforeSleepProc(eventLoop, beforeSleepProc);

    for(int i = 1; i <= g_iTimerNum; ++i)
    {
        int iTime = rand() % 10 + 1;    //1~10 ms
        // Log("[%d] rand time: %d ms", i - 1, iTime);
    #ifdef  USE_AE3
        long long lTimerId = aeCreateTimeEvent(eventLoop, iTime, evTimeProc_3, (void*)(new CTimeVal), NULL);
    #else
        long long lTimerId = aeCreateTimeEvent(eventLoop, iTime, evTimeProc, (void*)(new CTimeVal), NULL);
    #endif
    }

    CTimeVal oTimer;
    aeMain(eventLoop);
    Log("aeMain finish, cost time: %lld ms", oTimer.CostTime());

    aeDeleteEventLoop(eventLoop);


    Log("---------------------------main finish, cost time: %lld ms-----------------------------", oMainTimer.CostTime());

    return 0;
}
