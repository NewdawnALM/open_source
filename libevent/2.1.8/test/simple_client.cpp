#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <iostream>
#include <vector>
#include <set>
#include <map>
using std::cin;
using std::vector;
using std::set;
using std::map;

#include "event.h"
#include "event2/util.h"


typedef struct sockaddr SA;

int tcp_connect_server(const char* server_ip, int port)
{
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    int status = inet_aton(server_ip, &server_addr.sin_addr);
 
    if(status == 0)   //the server_ip is not valid value
    {
        errno = EINVAL;
        return -1;
    }
 
    int sockfd = ::socket(PF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
    {
        return sockfd;
    }

    status = ::connect(sockfd, (SA*)&server_addr, sizeof(server_addr) );
 
    if( status == -1 )
    {
        int save_errno = errno;
        ::close(sockfd);
        errno = save_errno; //the close may be error
        return -1;
    }
 
    evutil_make_socket_nonblocking(sockfd);
 
    return sockfd;
}

void read_from_stdin_cb(int fd, short events, void* arg)
{
    printf("%s|%d| fd: %d, events: %d\n", __FUNCTION__, __LINE__, fd, events);

    char msg[1024];
 
    int ret = read(fd, msg, sizeof(msg));
    if(ret <= 0)
    {
        perror("read fail");
        exit(1);
    }

    int sockfd = *((int*)arg);

    if(strncmp(msg, "bye", 3) == 0)
    {
        printf("%s|%d| bye bye~\n", __FUNCTION__, __LINE__);
        evutil_closesocket(sockfd);
        return;
    }
    //把终端的消息发送给服务器端
    //为了简单起见，不考虑写一半数据的情况
    write(sockfd, msg, ret);
}

void socket_read_cb(int fd, short events, void *arg)
{
    printf("%s|%d| fd: %d, events: %d\n", __FUNCTION__, __LINE__, fd, events);

    char msg[1024];
    //为了简单起见，不考虑读一半数据的情况
    int len = read(fd, msg, sizeof(msg) - 1);

    if(len <= 0)
    {
        perror("read fail");
        exit(1);
    }

    msg[len] = '\0';
    printf("%s|%d| recv[%s] from server\n", __FUNCTION__, __LINE__, msg);
}
 
int main(int argc, char** argv)
{
    if(argc < 3)
    {
        printf("usage: %s [ip] [port]\n", argv[0]);
        return -1;
    }

    //两个参数依次是服务器端的IP地址、端口号
    int sockfd = tcp_connect_server(argv[1], atoi(argv[2]));
    if(sockfd == -1)
    {
        perror("tcp_connect error");
        return -1;
    }
    printf("%s|%d| connect to server successful\n", __FUNCTION__, __LINE__);

    // while(true)     //常规tcp客户端，一问一答形式
    // {
    //     printf("wait to read from stdin...\n");

    //     char msg[2048];
    //     int len = read(STDIN_FILENO, msg, 2048);
    //     if(len <= 0)
    //     {
    //         printf("%s|%d read from stdin error\n", __FUNCTION__, __LINE__);
    //         exit(1);
    //     }
    //     if(strncmp(msg, "bye", 3) == 0)
    //     {
    //         close(sockfd);
    //         printf("close fd[%d]\n", sockfd);
    //         return 0;
    //     }
    //     int w_res = write(sockfd, msg, len);
    //     printf("w_res: %d\n", w_res);

    //     //因为上面把sockfd设置为了非阻塞，所以这里第一次会立刻返回一个EAGAIN的错误，后续都会返回上一次write的结果
    //     int r_res = read(sockfd, msg, 2048);

    //     if(r_res >= 0)
    //     {
    //         msg[r_res] = '\0';
    //         printf("r_res: %d, msg: %s\n", r_res, msg);
    //     }
    //     else
    //     {
    //         printf("r_res: %d, errno: %d, (EAGAIN: %d)\n", r_res, errno, EAGAIN);
    //         perror("read from sockfd error");
    //     }
    // }
 
    struct event_base* base = event_base_new();
 
    struct event *ev_sockfd = event_new(base, sockfd, EV_READ | EV_PERSIST, socket_read_cb, NULL);

    event_add(ev_sockfd, NULL);
    //监听终端输入事件
    struct event* ev_cmd = event_new(base, STDIN_FILENO, EV_READ | EV_PERSIST, read_from_stdin_cb, (void*)&sockfd);

    event_add(ev_cmd, NULL);
 
    event_base_dispatch(base);
 
    printf("finished \n");
    return 0;
}
