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
#define TIEMR_HEAP_ALLOW

#include "threadpool.h"
#include "http_request.h"
#include "util.h"

/**
 * 统一事件源就是信号处理不是在单独的信号处理函数中，
 * 而是将信号处理逻辑放到同一I/O复用处理逻辑中
 * 
 * 
*/
static int epoll_num =0;
static int pipefd[2];
#define MAX_EVENt_NUMBER 1024
#define MAX_FD 65536
HttpRequest* requests = nullptr;

//信号处理函数
void sig_handler(int sig){
    int saved_errno = errno;
    int msg = sig;
    send(pipefd[1], (char* )&msg, 1, 0); //只发送一个字节
    errno = saved_errno;
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


void timer_handler(){

    int start = TimerHeapThreadSafe::GetInstance()->Tick();
    //获取堆顶的时钟，定时下一次
    int interval = TimerHeapThreadSafe::GetInstance()->MinTime(start);
    
    alarm(interval);
}

int config_listen(char *cip, char* cport){


    int port = atoi(cport);

    //1. 创建socket fd
    int listen_fd = socket(PF_INET, SOCK_STREAM, 0); //创建socket，Protocol family
    assert(listen_fd >= 0);

    //2. 创建地址
    struct sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    inet_pton(AF_INET, cip, &sock_addr.sin_addr);
    sock_addr.sin_port = htons(port);

    int ret = 0;
    
    //3. socket fd 与地址绑定
    int optval = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    ret = bind(listen_fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    assert(ret != -1);

    //4. 监听socket
    ret = listen(listen_fd, 5);
    assert(ret != -1);
    return listen_fd;
}

int accept_conn(int listenfd, int epollfd){
    int conn_fd = accept(listenfd, nullptr, nullptr);
    if(conn_fd <= 0)
        return -1;
    addfd(epollfd, conn_fd, EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP | EPOLLHUP,  true); 
    #ifdef TIEMR_HEAP_ALLOW
    HttpRequest* new_request = requests + conn_fd;
    Timer* timer = new Timer(new_request, TIME_OUT);
    //初始化请求
    requests[conn_fd].InitRequest(epollfd, conn_fd, timer);
    //添加计时器
    TimerHeapThreadSafe::GetInstance()->AddTimer(timer);
    //如果之前由计时说明已有计时器启动，则继续计时，若没有，则新开启一个计时
    int pre = alarm(0);
    if(pre == 0){
        alarm(TIME_OUT);
    }
    else{
        alarm(pre);
    }
    #else
    requests[conn_fd].InitRequest(epollfd, conn_fd, nullptr);
    #endif
    return 0;
}

int main(int argc, char **argv){
    if(argc <= 2){
        printf("usage: %s ip_address port_number", argv[0]);
        exit(-1);
    }

    //1. 创建监听socket
    int listenfd = config_listen(argv[1], argv[2]);

    //2.创建内核事件监听表
    struct epoll_event events[MAX_EVENt_NUMBER];
    int epollfd = epoll_create1(0); 
    assert(epollfd != -1);

    //3. 注册epoll监听事件，监听listen端口
   // addfd(epollfd, listenfd, false);
    epoll_event event;
    event.data.fd = listenfd;
    event.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event);
    setnonblocking(listenfd);
    addfd(epollfd, listenfd,  EPOLLIN, false);
    //4. 创建客户请求数组，使用sockfd下标访问
    requests = new HttpRequest[MAX_FD];
    //5. 创建线程池
    ThreadPool * pool = ThreadPool::GetInstance(5);

    //查看timer_heap
    TimerHeapThreadSafe * timer_heap_test = TimerHeapThreadSafe::GetInstance();

    int ret = 0;
    //6. 统一事件源
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);

    //7. 注册信号事件
    addsig(SIGALRM, sig_handler);
    addfd(epollfd, pipefd[0],EPOLLIN | EPOLLET, false);

    bool stop_server = false;
    bool time_out = false;
    while(!stop_server){
        
        time_out = false;
        int number = epoll_wait(epollfd, events, MAX_EVENt_NUMBER, -1);
        // printf("epoll_number: %d\n", ++epoll_num);
        if(number < 0 && errno != EINTR){ //被信号中断时设置errno为EINTR X, 信号一般开启RESTART，能恢复被中断的系统调用
            printf("epoll_wait failure\n");
            exit(-1);
        }

        for(int i = 0; i < number; ++i){
            int sockfd = events[i].data.fd;
            
            //Accept
            if(sockfd == listenfd){
                accept_conn(listenfd, epollfd);
            }
            //信号处理
            else if(sockfd == pipefd[0] && events[i].events & EPOLLIN){
                char signal[1024];

                int ret = recv(pipefd[0], signal, sizeof(signal), 0);
                if(ret == -1 || ret == 0){ //没有读到信号,出错 //信号读取完
                    continue;
                }
                else{//ret为接收到信号的数量
                    for(int j = 0; j < ret; ++j){
                        switch (signal[j])
                        {
                        case SIGALRM:
                            time_out = true;
                            break;
                        default:
                            stop_server = true;
                        }
                    }

                }

            }
            //客户连接关闭
            else if(events[i].events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)){
                requests[sockfd].EndRequest();
            }
            //客户端连接
            else if(events[i].events & EPOLLIN){
                //不能添加addfd(epollfd, sockfd, (EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLHUP), false);
                pool->Append(requests + sockfd);
            }
        }

        //定时事件优先级比I/O事件低，可能造成一点偏差
        if(time_out == true){
            timer_handler();
        }
    }

    delete [] requests;
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);

    return 0;
}
