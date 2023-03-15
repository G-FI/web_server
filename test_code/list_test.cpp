#include <list>
#include <iostream>
#include "../src/http_request.h"
#include "../src/threadpool.h"

using namespace std;

int test_list(){

    int num = 65535;

    list<HttpRequest*> requests;
    
    for(int i = 0; i < num; ++i){
        HttpRequest* tmp = new HttpRequest();
        requests.push_back(tmp);
    }

    for(int i = 0; i < num; ++i){
        HttpRequest* tmp = requests.front();
        requests.pop_front();
        delete tmp;
    }
    return 0;
}

int main(){
    int num = 10000;
    ThreadPool* pool = ThreadPool::GetInstance(5);

    for(int i = 0; i < num; ++i){
        HttpRequest* tmp = new HttpRequest();
        pool->Append(tmp);
    }

    return 0;

}