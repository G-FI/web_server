1. listenfd不能设置为ET触发，会导致epoll_wait不会返回
2. ```C++
    /*不能先关闭连接，如果先关闭，则该sockfd可能被宠用，在释放this->timer阶段，导致当前连接的timer没有释放掉，却被另一个连接的timer进行初始化，此时要么当前this->timer还没构造好，导致delete this->timer出错，或者已经构造好，但是this->timer却被删除了，导致该连接没有定时器了，之后在timerheap中操作该定时器时也会报错
    */
    void closeConnection(){
        if(this->epollfd != -1){
            removefd(this->epollfd, this->sockfd);
            close(this->sockfd);
            printf("close >: %d\n", this->sockfd);
        }

        this->epollfd = -1;
        this->sockfd = -1; 
        //有可能正在处理请求中，有映射的文件，但是定时器时间到需要清除
        if(file_addr){
            munmap(this->file_addr, this->file_size);
            file_addr = nullptr;
        }

        //timer对象由接受客户连接时，分配
        //关闭连接时释放
        if(this->timer){
            delete this->timer;
        }
        this->timer = nullptr;
    }
3. 添加socket客户端连接描述符时，EPOLLERR EPOLLHUP EPOLLRDHUP都要监听，否则对端已经关闭连接之后，服务端不知道，不关闭连接就一直处于CLOSE_WAIT状态，导致文件描述符不够用了
4. 当一个请求的定时器是启动的，但是收到客户端断开连接时，会进行EndRequest()，会关闭掉连接，其中会释放timer对象，但是其在堆中，会在超时时被移除并调用DoTimer，但由于已经被关闭，所以Segment Falut
   1. 解决：在EndRequest()中先将自身定时器从TimerHeapThreadSafe中移除，再关闭连接，由于EndRequest()和timer_hander是串行，所以不会出现问题，即使是并行，也可以像处理HttpRquest中若成功移除timer，那么就进行closeConnection，否则就返回，将关闭操作交给DoTimer去


   2. 此时由于服务端**正在处理**(使用EPOLLET+EPOLLONESHOT)，，所以epoll收不到客户端的断开请求，也就无法在事件循环中调用EndRequest(),无法关闭连接
   3. 服务端处请求前将timer从heap中删除，所以定时器也无法关闭连接
   4. 客户端由于是长连接

5. 修改3之后还有个问题，在使用webbench测试时(后curl测试)客户端会主动关闭掉连接，此时由于服务端**正在处理**(使用EPOLLET+EPOLLONESHOT)，所以epoll收不到客户端的断开请求，也就无法在事件循环中调用EndRequest，导致连接无法关闭，处于close-wait状态
   1. 解决：处理客户连接事件之前，先将该sockfd的EPOLLERR | EPOLLRDHUP | EPOLLHUP事件添加，这样，就可能在处理过程中断开连接
6. 5错误，若正在处理客户事件时，删除关闭连接，那么正在处理的DoRequest中会出现错误
   1. 解决：TODO