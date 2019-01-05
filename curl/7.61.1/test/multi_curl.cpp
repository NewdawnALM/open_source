#include <string>  
#include <iostream>  
#include <sstream>
#include <vector>
#include <curl/curl.h>  
#include <sys/time.h>  
#include <unistd.h>  
#include <string.h>
#include <time.h>
#include <cstdlib>
#include <set>
#include "timeval.h"
#include <assert.h>
using namespace std;
#include "comdef.h"
#include "env_config.h"

#define  cout  cout<<__LINE__<<": "
#define  cerr  cerr<<__LINE__<<": "

template<class T>
string IntToStr(T num)
{
    stringstream ss;
    ss << num;
    return ss.str();
}

size_t curl_writer(void *buffer, size_t size, size_t count, void * stream)  
{  
    std::string * pStream = static_cast<std::string *>(stream);  
    (*pStream).append((char *)buffer, size * count);  
  
    return size * count;  
};  
  
/** 
 * 生成一个easy curl对象，进行一些简单的设置操作 
 */  
CURL * curl_easy_handler(const std::string &sUrl, const std::string &strHost, std::string &sRsp, unsigned int uiTimeout)
{
    CURL * curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, sUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

    if (uiTimeout > 0)
    {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, uiTimeout);
    }
    // if (!sProxy.empty())
    // {
    //     curl_easy_setopt(curl, CURLOPT_PROXY, sProxy.c_str());
    // }
    curl_slist *headers = NULL;
    headers = curl_slist_append(headers, string("Host: " + strHost).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sRsp);

    return curl;
}

CURL* get_tcp_handler()     // 这个是错的
{
    CURL *curl = curl_easy_init();
    if(!curl) {
        cout << "curl is null!\n";
        exit(3);
    }
    curl_easy_setopt(curl, CURLOPT_URL, "10.123.9.164");
    curl_easy_setopt(curl, CURLOPT_PORT, 9008);
    /* Do not do the transfer - only connect to host */
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
    string request = "hello";
    size_t iolen = 0;
    CURLcode res = curl_easy_send(curl, request.c_str(), request.length(), &iolen);
    if(CURLE_OK != res)
    {
        cout << "Error: " << curl_easy_strerror(res) << "\n";
        exit(3);
    }
    return curl;
}

void out_fd_set(fd_set _fd)
{
    for(int i = __FD_SETSIZE / __NFDBITS - 1; i >= 0; --i)
    {
        printf("%d ", __FDS_BITS(&_fd)[i]);
    }
    puts("");
}

/** 
 * 使用select函数监听multi curl文件描述符的状态 
 * 监听成功返回0，监听失败返回-1 
 */  
int curl_multi_select(CURLM * curl_m)  
{  
    int ret = 0;
    struct timeval timeout_tv;
    fd_set  fd_read;
    fd_set  fd_write;
    fd_set  fd_except;
    int     max_fd = -1;
  
    // 注意这里一定要清空fdset, curl_multi_fdset不会执行fdset的清空操作
    FD_ZERO(&fd_read);  
    FD_ZERO(&fd_write);  
    FD_ZERO(&fd_except);  

    // 设置select超时时间
    timeout_tv.tv_sec = 1;  
    timeout_tv.tv_usec = 0;  
  
    // 获取multi curl需要监听的文件描述符集合 fd_set，其实这个函数应该叫 curl_multi_fdget 可能更符合其功能，因为是取值而不是设置值
    curl_multi_fdset(curl_m, &fd_read, &fd_write, &fd_except, &max_fd);  

    Log("fd_read:");
    out_fd_set(fd_read);
    // out_fd_set(fd_write);
    Log("max_fd: %d", max_fd);

    /** 
     * When max_fd returns with -1, 
     * you need to wait a while and then proceed and call curl_multi_perform anyway. 
     * How long to wait? I would suggest 100 milliseconds at least, 
     * but you may want to test it out in your own particular conditions to find a suitable value. 
     */  
    if (-1 == max_fd)  
    {  
        return -1;  
    }
    /**
     * 执行监听，当文件描述符状态发生改变的时候返回
     * 返回0，程序调用curl_multi_perform通知curl执行相应操作
     * 返回-1，表示select错误
     * 注意：即使select超时也需要返回0，具体可以去官网看文档说明
     */
    int ret_code = ::select(max_fd + 1, &fd_read, &fd_write, &fd_except, &timeout_tv);
    Log("select ret_code: %d", ret_code);
    out_fd_set(fd_read);

    switch(ret_code)  
    {
        case -1:
            /* select error */
            ret = -1;
            break;
        case 0:
            /* select timeout */
        default:
            /* one or more of curl's file descriptors say there's data to read or write*/
            ret = 0;
            break;
    }
    // puts("");
    return ret;  
}  

// 这里设置超时时间  //  
unsigned int    TIMEOUT = 4000; /* ms */  
  
/** 
 * multi curl使用demo 
 */  
int curl_multi_demo(const int num)  
{  
    // 初始化一个multi curl 对象 //  
    CURLM * curl_m = curl_multi_init();  
  
    std::string RspArray[num];
    CURL * CurlArray[num];
    std::set<int> setCurlIds;
  
    vector<int> vecSleep{100, 200, 300, 200};
    // 设置easy curl对象并添加到multi curl对象中  //  
    for (int idx = 0; idx < num; ++idx)  
    {  
        // CurlArray[idx] = curl_easy_handler(g_strUrlPrefix + IntToStr(idx + 1), g_strHost, RspArray[idx], TIMEOUT);
        CurlArray[idx] = curl_easy_handler(g_strUrlPrefix + IntToStr(vecSleep[idx]), g_strHost, RspArray[idx], TIMEOUT);
        assert(CurlArray[idx] != NULL);
        CURLMcode mCodeRes = curl_multi_add_handle(curl_m, CurlArray[idx]);
        assert(mCodeRes == CURLM_OK);
        setCurlIds.insert(idx);
    }
    /* 
     * 调用curl_multi_perform函数执行curl请求 
     * iRunningHandles变量返回正在处理的easy curl数量，iRunningHandles为0表示当前没有正在执行的curl请求 
     */  
    int iRunningHandles;
    bool bFinish = false;
    do {
        // 监听到事件，调用curl_multi_perform通知curl执行相应的操作
        CURLMcode mCodeRes = curl_multi_perform(curl_m, &iRunningHandles);
        if(mCodeRes != CURLM_OK)
        {
            Log("multi wait error: %d", mCodeRes);
            exit(2);
        }
        Log("iRunningHandles: %d, time: %lu", iRunningHandles, time(NULL));

        int msgs_left;
        CURLMsg * msg;
        while(msg = curl_multi_info_read(curl_m, &msgs_left))
        {
            Log("msg not null");
            if (CURLMSG_DONE == msg->msg)
            {
                int idx;
                for (idx = 0; idx < num; ++idx)
                {
                    if (msg->easy_handle == CurlArray[idx])   break;
                }
                if (idx == num)
                {
                    Log("curl not found");
                    exit(1);
                }
                else
                {
                    Log("curl [%d] completed with status: %d, response: %s", idx, msg->data.result, RspArray[idx].c_str());
                    if(idx == 0)
                    {
                        // bFinish = true;
                    }
                    curl_multi_remove_handle(curl_m, CurlArray[idx]);
                    curl_easy_cleanup(CurlArray[idx]);
                    setCurlIds.erase(idx);
                }
            }
        }
        if(bFinish)  break;
        // 为了避免循环调用 curl_multi_perform 产生的cpu持续占用的问题，采用select来监听文件描述符
        // if (-1 == curl_multi_select(curl_m))  break;
        int iNumfds = 0;
        //如果不是访问本机的http这个接口会执行多次，猜测是获取到socket的可写入事件时返回的
        mCodeRes = curl_multi_wait(curl_m, NULL, 0, 1000, &iNumfds);
        if(mCodeRes != CURLM_OK)
        {
            Log("multi wait error: %d", mCodeRes);
            exit(2);
        }
        Log("iNumfds: %d, time: %lu", iNumfds, time(NULL));
    }
    while(iRunningHandles > 0);

    for (set<int>::const_iterator cit = setCurlIds.begin(); cit != setCurlIds.end(); ++cit)
    {
        curl_multi_remove_handle(curl_m, CurlArray[*cit]);
        curl_easy_cleanup(CurlArray[*cit]);
    }
    curl_multi_cleanup(curl_m);

    return 0;
}  
  
/** 
 * easy curl使用demo 
 */  
int curl_easy_demo(int num)  
{
    std::string RspArray[num];
  
    for (int idx = 0; idx < num; ++idx)  
    {
        CURL * curl = curl_easy_handler(g_strUrlPrefix + IntToStr(rand() % 100 + 1), g_strHost, RspArray[idx], TIMEOUT);
        CURLcode code = curl_easy_perform(curl);
        cout << "curl [" << idx << "] completed with status: " << code << endl;
        cout << "rsp: " << RspArray[idx] << endl;
        curl_easy_cleanup(curl);
    }
    return 0;
}
  
int main(int argc, char * argv[])  
{  
    if (argc < 3)  
    {  
        cout << "usage: " << argv[0] << " [1-easy/2-multi] [http request num]\n";
        return -1;  
    }  
    int iType = atoi(argv[1]);
    int num = atoi(argv[2]);
  
    CTimeVal oTimeVal;  //开始计时

    if(iType == 2)
    {
        // 使用multi接口进行访问
        curl_multi_demo(num);
    }
    else
    {
        // 使用easy接口进行访问
        curl_easy_demo(num);
    }
    Log("cost time: %ldms", oTimeVal.costTime());

    return 0;  
}
