#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
 
#include <unistd.h>
#include "event.h"

void socket_read_cb(int fd, short events, void *arg)
{
    printf("%s|%d| fd: %d, event: %d\n", __FUNCTION__, __LINE__, fd, events);

    char msg[4096];
    int len = read(fd, msg, sizeof(msg) - 1);

    if(len <= 0)
    {
        if(len < 0)
        {
            printf("len = %d, errno: %d\n", len, errno);
            perror("some error happen when read");
        }
        event_free((struct event*)arg);
        close(fd);
        return ;
    }

    msg[len] = '\0';
    printf("%s|%d| recv the client[%d] msg: %s", __FUNCTION__, __LINE__, fd, msg);
 
    char reply_msg[4096] = "Server have recvieced the msg: ";
    strcat(reply_msg + strlen(reply_msg), msg);
 
    write(fd, reply_msg, strlen(reply_msg) );
}

void accept_cb(int fd, short events, void* arg)
{
    printf("%s|%d| fd: %d, events: %d\n", __FUNCTION__, __LINE__, fd, events);

    evutil_socket_t sockfd;
 
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
 
    sockfd = ::accept(fd, (struct sockaddr*)&client, &len );
    evutil_make_socket_nonblocking(sockfd);
 
    printf("%s|%d| accept a client %d\n", __FUNCTION__, __LINE__, sockfd);
 
    struct event_base* base = (event_base*)arg;
 
    //仅仅是为了动态创建一个event结构体
    struct event *ev = event_new(NULL, -1, 0, NULL, NULL);
    //将动态创建的结构体作为event的回调参数
    event_assign(ev, base, sockfd, EV_READ | EV_PERSIST, socket_read_cb, (void*)ev);
 
    event_add(ev, NULL);
}

typedef struct sockaddr SA;
int tcp_server_init(int port, int listen_num)
{
    int errno_save;
    evutil_socket_t listener;
 
    listener = ::socket(AF_INET, SOCK_STREAM, 0);
    if( listener == -1 )
    {
        return -1;
    }
    //允许多次绑定同一个地址。要用在socket和bind之间
    evutil_make_listen_socket_reuseable(listener);
 
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(port);
 
    if( ::bind(listener, (SA*)&sin, sizeof(sin)) < 0 )
        goto error;
 
    if( ::listen(listener, listen_num) < 0)
        goto error;
 
 
    //跨平台统一接口，将套接字设置为非阻塞状态
    evutil_make_socket_nonblocking(listener);
 
    return listener;
 
    error:
        errno_save = errno;
        evutil_closesocket(listener);
        errno = errno_save;
 
        return -1;
}
 
int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("usage: %s [port]\n", argv[0]);
        exit(1);
    }
    int listener = tcp_server_init(atoi(argv[1]), 10);
    printf("%s|%d| server socket fd: %d\n", __FUNCTION__, __LINE__, listener);

    if( listener == -1 )
    {
        perror("tcp_server_init error");
        return -1;
    }
 
    struct event_base* base = event_base_new();
    //添加监听客户端请求连接事件
    struct event* ev_listen = event_new(base, listener, EV_READ | EV_PERSIST, accept_cb, base);

    event_add(ev_listen, NULL);
 
    event_base_dispatch(base);
 
    return 0;
}

