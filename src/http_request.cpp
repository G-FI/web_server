#include "http_request.h"
#include "util.h"
#include <thread>
#define TIEMR_HEAP_ALLOW

const char* HttpRequest::not_found_path = "/home/hpy/var/www/html/not_found.html";
const char* HttpRequest::root = "/home/hpy/var/www/html";

HttpRequest::HttpRequest(){
}

HttpRequest::~HttpRequest(){}

void HttpRequest::DoRequest(){
     bool ret;
    #ifdef TIEMR_HEAP_ALLOW
    //0. 从定时器堆中删除自身定时器
    ret = TimerHeapThreadSafe::GetInstance()->RemoveTimer(this->timer);
    
    if(ret == false){   //该请求已经/正在被定时器处理，请求已过期
        return;
    }

    // fprintf(stderr, "Get The Reqeust, Remove the Timer\n");
    #endif
    printf("thread: %d < Doing %d \n",std::this_thread::get_id(), this->sockfd);
    //1. 读取数据
    ret = this->read();
    if(!ret){
        this->closeConnection();
        return;
    }

    //2. 解析数据
    HTTP_STATUS status = this->parseHttp();

    //3.1 请求头还没有接受完,重新注册sockfd上的读事件(主要是清除EPOLLONESHOT)
    if(status == NO_REQUEST){
        modfd(this->epollfd, this->sockfd, EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP | EPOLLHUP);  
        #ifdef TIEMR_HEAP_ALLOW
        this->timer->Reset(this, TIME_OUT);
        TimerHeapThreadSafe::GetInstance()->AddTimer(this->timer);
        #endif

        return;
    }
    //3.2 解析结束，doRequest查找对应的请求文件，并映射
    else if(status == GET_REQUEST){
        status = this->doRequest();
    }

    //4. 处理请求
    this->doRespond(status);

    //5. 写回数据
    this->write();

    //6. 写完数据根据是否长连接来决定关闭
    #ifdef TIEMR_HEAP_ALLOW
    if(this->keep_alive){
        this->timer->Reset(this, TIME_OUT);
        TimerHeapThreadSafe::GetInstance()->AddTimer(this->timer);
        //重新将该sockfd添加到监听表中
        modfd(this->epollfd, this->sockfd, EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP | EPOLLHUP);    
    }
    else{
        this->closeConnection();
    }
    #else
    this->closeConnection();
    #endif
}

void HttpRequest::DoTimer(){
    fprintf(stderr, "[HTTP Connection Timer done]: %d\n ", this->sockfd);
    this->closeConnection();
}
void HttpRequest::EndRequest(){
    TimerHeapThreadSafe::GetInstance()->RemoveTimer(this->timer);
    this->closeConnection();
}

void HttpRequest::InitRequest(int epollfd, int sockfd, Timer* timer_){
    //fd相关
    this->epollfd = epollfd;
    this->sockfd = sockfd;

    //请求数据相关
    this->check_idx = 0;
    this->read_idx = 0;
    this->line_idx = 0;
    this->cur_state = CHECK_STATUS_REQUESTLINE;
    memset(this->read_buf, 0, BUFFERSZ);

    //方法
    this->method = GET;
    this->uri.resize(0);
    this->host.resize(0);
    this->keep_alive = true;

    //响应数据相关
    this->write_size = 0;
    this->file_addr = nullptr;
    this->file_size = 0;
    memset(this->write_buf, 0, BUFFERSZ);    

    //定时器
    this->timer = timer_;
}


//解析read_buf中的一行数据
//1. 若解析完返回LINE_OK
//2. 未遇见\r\n则返回LINE_OPEN
//3. 其他情况返回  LINE_BAD
HttpRequest::LINE_STATUS HttpRequest::parseLine(){
    char ch;
    for(; check_idx < read_idx; ++check_idx){
        ch = read_buf[check_idx];

        if(ch == '\r'){
            if(check_idx + 1 == read_idx){
                return LINE_OPEN;
            }
            else if(read_buf[check_idx+1] != '\n'){
                return LINE_BAD;
            }
            else{
                //正确的解析了一行
                check_idx += 2;
                read_buf[check_idx-1] = 0;
                read_buf[check_idx-2] = 0;
                return LINE_OK;
            }
        }
        else if( ch == '\n'){
            if(check_idx == 0 ||(check_idx >0 && read_buf[check_idx - 1] != '\r')){
                return LINE_BAD;
            }
            else{
                check_idx += 1;
                read_buf[check_idx-1] = 0;
                read_buf[check_idx-2] = 0;
                return LINE_OK;
            }
        }
    }
    //当前行还没接收完
    return LINE_OPEN;
}

//根据输入的一行，判断是否是正确的请求头，将请求行中对应的字段，写入到对应字段
// 返回NO_REQUEST请求未处理完
HttpRequest::HTTP_STATUS HttpRequest::parseHttpRequestLine(char *line){
    const char *space = " ";
    char * filed = strtok(line, space);

    //方法错误/不支持
    if(strcasecmp(filed, "GET") != 0){
        fprintf(stderr, "[HTTP PARSE LINE]: %s method is not supported\n", filed);
        return BAD_REQUEST;
    }

    filed = strtok(nullptr, space);

    //没有URI字段
    if(filed == nullptr || filed[0] != '/'){
        fprintf(stderr, "[HTTP PARSE LINE]: no URI\n");
        return  BAD_REQUEST;
    }

    this->uri = filed;
    
    filed  = strtok(nullptr, space);
    if(filed != nullptr && strcasecmp(filed, "HTTP/1.1") != 0){
        fprintf(stderr, "[HTTP PARSE LINE]: %s HTTP version not support\n", filed);
        return BAD_REQUEST;
    }


    filed = strtok(nullptr, space);
    if(filed != nullptr){
        fprintf(stderr, "[HTTP PARSE LINE]: %s , bad request line\n", filed);
        return BAD_REQUEST;
    }

    return NO_REQUEST;
}

//input: 解析的一行以0结尾的字符
//output: GET_REQUEST 请求头处理完毕
//        ON_REQUEST  请求头还未处理完
HttpRequest::HTTP_STATUS HttpRequest::parseHttpRequestHeader(char *line){
    //由于parseline会将每一行数据以0结尾分隔开，所以请求头和消息体中间的 <CR><LR>对应的一行为0

    //目前只支持解析请求头，没有消息体
    if(line[0] == 0){
        return GET_REQUEST;
    }
    if(strncasecmp(line, "Host:", 5) == 0){
        line += 5;
        //line指向主机名
        line += strspn(line, " "); //返回第一个与 accept不相同的位置
        this->host = line;
    }
    else if(strncasecmp(line, "Connection:", 11) == 0){
        line += 11;
        line += strspn(line, " ");
        if(strncasecmp(line, "close", 5) == 0){
            this->keep_alive = false;
        }
    } 
    else{
        //虽然没出现header，但并不代表出错
        //fprintf(stderr, "[HTTP PARSE HEADER]: unkonwn header %s\n", line);
    }
    return NO_REQUEST;
}

//TODO: 暂时不支持请求体
HttpRequest::HTTP_STATUS HttpRequest::parseHttpRequestBody(char *text){
    return GET_REQUEST;
}

//前置条件:将数据读取到read_buf中
//output:  NO_REQUEST http请求解析未结束
//         GET_REQUEST 解析成功
//         INTERNAL_ERROR   解析出错
HttpRequest::HTTP_STATUS HttpRequest::parseHttp(){

    LINE_STATUS line_status = LINE_OK;
    HTTP_STATUS ret = NO_REQUEST;
    char * line;

    while((line_status=parseLine()) == LINE_OK){//成功解析一行请求  
        line = getNextLine();
        //fprintf(stdout, "[PARSING LINE]: %s\n", line);

        switch (this->cur_state)
        {
        case CHECK_STATUS_REQUESTLINE:
            ret = this->parseHttpRequestLine(line);
            //请求行解析出错
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            //成功时改变状态
            else{
                this->cur_state = CHECK_STATUS_HEADER;
            }
            break;
        case CHECK_STATUS_HEADER:
            ret = this->parseHttpRequestHeader(line);
            if(ret == GET_REQUEST){
                //this->cur_state = CHECK_STATUS_BODY;
                return GET_REQUEST;
            }
            break;
        case CHECK_STATUS_BODY:
            
            break;
        default:
            return INTERNAL_ERROR; 
            break;
        }
    }
    return NO_REQUEST;
}

//对静态文件处理请求之后可能的状态
//NO_RESOURCE          
//FORBIDDEN_REQUEST
//BAD_REQUEST
//FILE_REQUEST
HttpRequest::HTTP_STATUS HttpRequest::doRequest(){
    string file_path = this->root + uri;

    struct stat file_stat;
    //没有对应文件
    if(stat(file_path.data(), &file_stat) < 0){
        
        stat(this->not_found_path, &file_stat);
        int fd = open(this->not_found_path, O_RDONLY);
        this->file_addr = (char*)mmap(nullptr, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        this->file_size = file_stat.st_size;
        close(fd);
        return NO_RESOURCE;
    }
    //其他用户是否有读权限
    else if(!(file_stat.st_mode &  S_IROTH)){
        return FORBIDDEN_REQUEST;
    }
    //不是常规文件，请求出错
    else if(!S_ISREG(file_stat.st_mode)){
        return BAD_REQUEST;
    }

    int fd = open(file_path.data(), O_RDONLY);
    this->file_addr = (char*)mmap(nullptr, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    this->file_size = file_stat.st_size;
    close(fd);

    return FILE_REQUEST;
}



bool HttpRequest::makeRespond(const char* http_code, const char* title){
    //添加相应行
    this->write_size += sprintf(write_buf, "HTTP/1.1 %s %s\r\n", http_code, title);
    
    //添加响应头
    this->write_size += sprintf(write_buf+this->write_size, "Connection: %s\r\n", this->keep_alive? "keep-alive":"close");
    
    //添加空行
    this->write_size += sprintf(write_buf+this->write_size, "\r\n");

    return true;
}


bool HttpRequest::doRespond(HTTP_STATUS status){
    switch (status)
    {
    case INTERNAL_ERROR:
        makeRespond("500", "Internal Error");

        break;
    case NO_RESOURCE:
        makeRespond("404", "Not Found");

        break;
    case BAD_REQUEST:
        makeRespond("400", "Bad Request");
        break;
    case FORBIDDEN_REQUEST:
        makeRespond("403", "Forbidden");

        break;
    case FILE_REQUEST:
        makeRespond("200", "OK");

        break;
    default:
        return false;
        break;
    }

    return true;
}



//读取当前请求socket上的数据
//后置条件：根据返回值 1. true: 接着进行后续的请求解析 2.false: 说明读取失败，关闭连接
bool HttpRequest::read(){

   //sockfd是非阻塞，并且EPOLL时间未 EPOLLIN | EPOLLONESHOT | EPOLLET
    while(true){
            if(this->read_idx > BUFFERSZ)
                return false;

            int len = recv(this->sockfd, this->read_buf+this->read_idx, BUFFERSZ - this->read_idx, 0);
            
            //读取失败情况 1. sockfd暂时没有数据返回true， 2. 其他情况返回false
            if(len == -1){
                if(errno == EWOULDBLOCK){
                    break;  
                }
                else{
                    return false;
                }
            }
            else if(len == 0){
                return false;
            }

            this->read_idx += len;
    }

   return true;
}

bool HttpRequest::write(){
    int len = 0;
    int nleft = this->write_size;
    char* bufp = this->write_buf;

    //手动进行集中写
    //如果socket阻塞时，采用轮询的方式，一次请求中一定要将数据发送给客户端
    while(nleft > 0){
        len = send(this->sockfd, bufp, nleft, 0);
        if(len == -1){
            if(errno == EWOULDBLOCK){
                len = 0;
            }
            else{
                return false;
            }
        }
        else if(len == 0){
            break;
        }
        bufp += len;
        nleft -= len;
    }
    

    if(file_addr == nullptr || file_size <= 0){
        return true;
    }

    //写静态文件
    nleft = this->file_size;
    bufp = this->file_addr;

    while(nleft > 0){
        len = send(this->sockfd, bufp, nleft, 0);
        if(len == -1){
            if(errno == EWOULDBLOCK){
                len = 0;
            }
            else{
                return false;
            }
        }
        else if(len == 0){
            break;
        }
        bufp += len;
        nleft -= len;
    } 
   
    //文件写完之后释放内存
    if(this->file_addr){
        munmap(this->file_addr, this->file_size);
        this->file_addr = nullptr;
        this->file_size = 0;
    }
    return true;
}



