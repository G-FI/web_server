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
