# ThreadPool
1. 使用单例，延迟创建
2. 生产者和消费者模式，使用mutex保护 request队列只能有一次只能由一个访问者，work线程等待在request队列上，**等待条件为!request.empty()**,当生产者添加一个请求时，随机通知一个等待在request队列上的工作线程来进行处理。
3. 线程池的用户满足**Request接口**

# 时间堆
1. 寻找要处理的定时器O(1)，插入一个定时器O(logN)

# 具体请求类 HttpRequest
1. 解析每一行数据的状态机 parseLine()
2. 解析请求头的状态机    parseHttpRequestLine()
3. 解析请求头的状态机    parseHttpRequestHeader()
4. 解析整个Http请求的状态机  parseHttp()

# 线程安全时间堆
1. 工作线程重置时间方法：
   1. 根据根据**timer元素**从堆中删除自身(保自己正在处理的Request不会被定时器清除掉)
   2. 处理Request
   3. 重新设置定时器添加到时间堆中
   
2. 工作线程删除自身时可能出现的情况：
   1. 已经被定时器处理，并且delete掉            DeleteTimer返回false
   2. 正在被定时器处理，但是还没有delete掉      DeleteTimer返回false
   3. 还没有被定时器处理                        DeleteTimer返回true，并将其从TimerHeap中删除     
   4. 在1，2情况下，工作线程放弃掉Request的处理
   5. 解决方案：
      1. erase_from_heap中检查两个(进入erase_from_heap时是占有锁的，所以是安全的)
         1. timer是否有效                               无效->对应情况1，返回false
         2. timer有效，但是timer->idx == INT_MIN        对应情况2返回false
         3. timer有效，并且timer->idx != INT_MIN        对应情况3返回true
3. HttpRequest负责Timer声明周期的管理

# BUG
1. 有时候Tick函数中从TimerHeap中获取到的定时器报错，已经被删除了，但是完全不可能啊？
   1. 原因1. 从堆中获取之前是加了锁的，那从堆中获取之后一定是没被其他线程修改过的，所以不可能有问题
   2. 原因2. Tick函数从Timerheap中将到期定时器取出来，一定是有效的，此时释放锁，其他线程在TimerHeap中是访问不到这个定时器的，所以保证这个定时器的有效性
   3. 综上，怎么会报段错误呢？

2. 服务端哪里没有关掉连接，使用ss -antp查看时服务端有许多close-wait
   1. 定时器到了一定会close掉
   2. 请求被处理时，会从定时器堆中移除该请求的定时器，处理完之后，若没有长连接则DoRequest中会关闭，若有长连接，则循环到1或2

3. 应该是锁的问题，将线程数量减到2个甚至1个时可以流畅运行
4. 也可能时epoll问题，因为即使使用curl进行访问服务器端口时，epoll_wait也不会返回
