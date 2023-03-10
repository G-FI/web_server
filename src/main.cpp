#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<errno.h>
#include<string.h>

#include<signal.h>

#include<fcntl.h>

#include<sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>

#include<sys/epoll.h>

#include<sys/wait.h>

#include "http_request.h"
#include "util.h"
#include "threadpool.h"

/**
 * 统一事件源就是信号处理不是在单独的信号处理函数中，
 * 而是将信号处理逻辑放到同一I/O复用处理逻辑中
 * 
 * 
*/


static int pipefd[2];
#define MAX_EVENt_NUMBER 1024
#define MAX_FD 65536

//信号处理函数
void sig_handler(int sig){
    int saved_errno = errno;
    int msg = sig;
    send(pipefd[1], (char* )&msg, 1, 0); //只发送一个字节
    errno = saved_errno;
}

//多进程编程，回收子进程资源
void sig_child_handler(int sig){
    int child_pid = waitpid(-1, NULL, WNOHANG);
}

//添加信号监听
void addsig(int sig, void (*handler)(int sig)){
    struct sigaction sa;

    memset(&sa, '\0', sizeof(sa));

    sa.sa_handler = handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);

    assert(sigaction(sig, &sa, NULL) != -1);
}



int main(int argc, char **argv){

    if(argc <= 2){
        printf("usage: %s ip_address port_number", argv[0]);
        exit(-1);
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int backlog = 5; //atoi(argv[3]);

    //1. 创建socket fd
    int listen_fd = socket(PF_INET, SOCK_STREAM, 0); //创建socket，Protocol family
    assert(listen_fd >= 0);

    //2. 创建地址
    struct sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &sock_addr.sin_addr);
    sock_addr.sin_port = htons(port);

    int ret = 0;
    
    //3. socket fd 与地址绑定
    int optval = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    ret = bind(listen_fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    assert(ret != -1);

    //4. 监听socket
    ret = listen(listen_fd, backlog);
    assert(ret != -1);

    //5.创建内核事件监听表
    struct epoll_event events[MAX_EVENt_NUMBER];
    int epollfd = epoll_create1(0); 
    assert(epollfd != -1);

    //5. 注册epoll监听事件，监听listen端口
    addfd(epollfd, listen_fd);

    HttpRequest* requests = new HttpRequest[MAX_FD];

    ThreadPool * pool = ThreadPool::GetInstance(5);

    bool stop_server = false;
    while(!stop_server){
        int number = epoll_wait(epollfd, events, MAX_EVENt_NUMBER, -1);

        if(number < 0 && errno != EINTR){ //被信号中断时设置errno为EINTR
            printf("epoll_wait failure\n");
            exit(-1);
        }

        for(int i = 0; i < number; ++i){
            int sockfd = events[i].data.fd;
            
            //处理监听
            if(sockfd == listen_fd){
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);
                if(conn_fd <= 0)
                    continue;
                addfd(epollfd, conn_fd); 
                requests[conn_fd].InitRequest(epollfd, conn_fd);
            }
            /*
            //信号处理
            else if(sockfd == pipefd[0] && events[i].events & EPOLLIN){
                char signal[1024];

                int ret = recv(pipefd[0], signal, sizeof(signal), 0);
                if(ret == -1){ //没有读到信号,出错
                    continue;
                }
                else if(ret == 0){//信号读取完
                    continue;
                }
                else{//ret为接收到信号的数量
                    for(int j = 0; j < ret; ++j){
                        printf("%d, %d\n", j, ret);
                        switch (signal[j])
                        {
                        case SIGCHLD:
                            printf("SIGCHLD\n");
                            continue;
                        case SIGHUP:
                            printf("SIGHUP\n");
                            continue;
                        case SIGTERM:
                        case SIGINT:
                            printf("STOP SERVER\n");
                            stop_server = true;
                            break;
                        }
                    }

                }

            }
            */
            //客户连接关闭
            else if(events[i].events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)){
                requests[sockfd].EndRequest();
            }
            //客户端连接
            else if(events[i].events & EPOLLIN){
                pool->Append(requests + sockfd);
            }

        }

    }

    delete [] requests;
    close(listen_fd);
    close(pipefd[0]);
    close(pipefd[1]);

    return 0;
}
