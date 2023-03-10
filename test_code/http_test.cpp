
#include <string.h>
#include <cstdio>
#include <stdlib.h>
#include <string>

#include "../http_request.h"

const char* request = "GET /index.html HTTP/1.1\r\nHost: Google chorme\r\nAccept: *\\*\r\n\r\n";

class HttpRequestTest: public HttpRequest{

public:
    void readData(){
        strncpy(this->read_buf, request, BUFFERSZ);
        this->read_idx = strnlen(read_buf, BUFFERSZ);
    }
};

enum HTTP_STATUS {NO_REQUEST, BAD_REQUEST, GET_REQUEST};

HTTP_STATUS parseHttpRequestLine( char *line){
    const char *space = " ";
    char * filed = strtok(line, space);

    //方法错误/不支持
    if(strcasecmp(filed, "GET") != 0){
        fprintf(stderr, "HTTP reqeuest line: %s method is not supported\n", filed);
        return BAD_REQUEST;
    }

    filed = strtok(nullptr, space);

    //没有URI字段
    if(filed == nullptr || filed[0] != '/'){
        fprintf(stderr, "HTTP reqeuest line: no URI\n");
        return  BAD_REQUEST;
    }

    std::string uri = filed;
    fprintf(stdout, "uri: %s\n", uri.data());
    filed  = strtok(nullptr, space);
    if(filed == nullptr || strcasecmp(filed, "HTTP/1.1") != 0){
        fprintf(stderr, "HTTP reqeuest line: %s HTTP version not support\n", filed);
        return BAD_REQUEST;
    }
    filed = strtok(nullptr, space);

    if(filed != nullptr){
        fprintf(stderr, "HTTP reqeuest line: %s , bad line\n", filed);
        return BAD_REQUEST;
    }

    return GET_REQUEST;
}


void test1(){

    char lines[7][40] = {
        "GET /index.html HTTP/1.1",
        "PUT /index.html HTTP/1.1",
        "GET / HTTP/1.1",
        "GET HTTP/1.1",
        "GET /index.html HTTP/1.0",
        "GET /index.html ",
        "GET /index.html HTTP/1.1 dummy",
    };


    for(int i = 0; i < 7; ++i){
        parseHttpRequestLine(lines[i]);
    }
    
}

//test state machine
int main(){
    HttpRequestTest test;

    test.readData();
    test.DoRequest();

    return 0;
}