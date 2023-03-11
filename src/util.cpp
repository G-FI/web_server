#include "util.h"

//设置对fd的读写位非阻塞
int setnonblocking(int fd){
    int old_fd_flag = fcntl(fd, F_GETFL); //获取fd原来的flag
    int new_fd_flag = old_fd_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_fd_flag);
    return old_fd_flag;
}

//将fd添加到内核创建的内核事件表epollfd中，将fd设置为非阻塞
void addfd(int epollfd, int fd, bool one_shot){
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET; //监测 数据可读事件 和 边沿触发(事件发生时只触发一次)
    if(one_shot){
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    //事件就绪 + 非阻塞I/O
    setnonblocking(fd);
}

//修改注册在fd上的监听事件
void modfd(int epollfd, int fd, int ev){
    
    struct epoll_event event;
    event.data.fd = fd;
    //EPOLLRDHUP发送简单的数据来监测对端是否已经关闭连接，若监测到EPOLLRDHUP
    //说明对方关闭，服务器端执行关闭操作
    event.events = ev | EPOLLIN | EPOLLET | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
}