#ifndef UTIL_H_
#define UTIL_H_

#include <fcntl.h>
#include <sys/epoll.h>

//设置对fd的读写位非阻塞
int setnonblocking(int fd);
//将fd添加到内核创建的内核事件表epollfd中，将fd设置为非阻塞
void addfd(int epollfd, int fd, unsigned int ev, bool one_shot);
//修改注册在fd上的监听事件 
void modfd(int epollfd, int fd, int ev);
void removefd(int epollfd, int fd);

#endif