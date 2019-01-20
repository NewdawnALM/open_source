#ifdef  USE_AE2
    #include "ae2.h"
#else
    #include "ae.h"
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
using namespace std;
#include "comdef.h"
#include "timeval.h"

int g_iTimerNum = 100000;

#define  _assert(bExpression) \
{ \
    assert(bExpression); \
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
    return AE_NOMORE;
}


void beforeSleepProc(struct aeEventLoop *eventLoop)
{
    // Log("beforeSleepProc begin, maxfd: %d, timeEventNextId: %lld", eventLoop->maxfd, eventLoop->timeEventNextId);

    if(eventLoop->maxfd < 0)
    {
        #ifdef  USE_AE2
            if(eventLoop->timeMinHead->size() == 0)
        #else
            if(eventLoop->timeEventHead == NULL)
        #endif
        {
            Log("here stop");
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
        long long lTimerId = aeCreateTimeEvent(eventLoop, iTime, evTimeProc, (void*)(new CTimeVal), NULL);
        // Log("lTimerId: %lld", lTimerId);
    }

    CTimeVal oTimer;
    aeMain(eventLoop);
    Log("aeMain finish, cost time: %lld ms", oTimer.CostTime());

    aeDeleteEventLoop(eventLoop);


    Log("---------------------------main finish, cost time: %lld ms-----------------------------", oMainTimer.CostTime());

    return 0;
}
