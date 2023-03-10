#ifndef HTTP_REQUEST_H_
#define HTTP_REQUEST_H_

#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "request.h"
#include "util.h"

using std::string;

#define BUFFERSZ 8124
#define FILE_NAME_LEN 100


enum{
    GET = 0,
    POST,
    PUT
};


class HttpRequest: public Request{
public:
    
    enum CHECK_STATUS {CHECK_STATUS_REQUESTLINE=0, CHECK_STATUS_HEADER, CHECK_STATUS_BODY};     //解析http请求的状态机状态
    enum LINE_STATUS {LINE_OK=0, LINE_OPEN, LINE_BAD} ;         //解析每一行字符的结果
    enum HTTP_METHOD {GET = 0};                                 
    enum HTTP_STATUS {NO_REQUEST=0, BAD_REQUEST, GET_REQUEST, INTERNAL_ERROR, NO_RESOURCE, FORBIDDEN_REQUEST,
                        FILE_REQUEST             //文件请求
                    };    //解析每一行请求的结果,以及服务请求的结果

private:
    char* getNextLine(){ int tmp = line_idx; line_idx = check_idx; return read_buf+tmp;}    //返回下一个应该处理的行，以0结尾,并更新下一个行的起点索引
    LINE_STATUS parseLine();
    HTTP_STATUS parseHttpRequestHeader(char *line);
    HTTP_STATUS parseHttpRequestLine(char *line);
    HTTP_STATUS parseHttpRequestBody(char *text);
    HTTP_STATUS parseHttp();

    HTTP_STATUS doRequest();
    bool doRespond(HTTP_STATUS status);

    bool makeRespond(const char* http_code, const char* title);

    void closeConnection(){
        removefd(this->epollfd, this->sockfd);
        close(this->sockfd);
    }
    
private:
    //Reactor模式，由工作线程自身读取数据  
    bool read();
    bool write();

public:
    
    HttpRequest();
    ~HttpRequest();

    virtual void InitRequest(int epollfd, int sockfd);
    virtual void DoRequest() override;
    virtual void DoTimer() override;
    virtual void EndRequest(){
        this->closeConnection();
    }

protected:
    static const char*           root;                 //web服务器的根目录
    static const char*           not_found_path;          //404文件路径
    char            read_buf[BUFFERSZ];
    int             sockfd;
    int             epollfd;
    size_t          check_idx;                 //当前解析位置
    size_t          read_idx;                  //当前buffer中字节数
    size_t             line_idx;                  //当前解析请求时，起始行的索引
    CHECK_STATUS     cur_state;


    //请求行，请求头 解析结果
    HTTP_METHOD             method;                 //请求方法
    string                  uri;                    //请求静态资源路径
    string                  host;                   //请求主机
    bool                    keep_alive;              //是否长连接

    //回复数据结果
    char                    write_buf[BUFFERSZ];    //响应行和头
    size_t                  write_size;
    char*                   file_addr;              //内存文件映射地址
    int                     file_size;
};



class HttpRespond{
public:
    HttpRespond();

    ~HttpRespond();

    void DoRespond();
private:
    string file_path;
};


#endif