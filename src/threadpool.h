#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <thread>
#include <list>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <assert.h>

#include "request.h"


using std::list;
using std::thread;
using std::vector;
using std::mutex;
using std::condition_variable;
using std::exception;
using std::unique_lock;

const int MAX_REQUEST = 10000;
const int MAX_THREAD  = 10;


class ThreadPool{
    void work();
    ThreadPool(int thread_number,int max_request);
public:
    static ThreadPool* GetInstance(int thread_number = MAX_THREAD, int max_request = MAX_REQUEST){
        if(instance == nullptr){
            instance = new ThreadPool(thread_number, max_request);
            
        }
        return instance;
    }
    ~ThreadPool();
    bool Append(Request* Request);

private:
    //静态单例
    static ThreadPool*      instance;

    int                     m_max_request;
    int                     m_thread_number;
    
    list<Request*>          m_request_queue; //工作队列
    vector<thread>          m_workers;     //工作线程

    condition_variable      m_cond;     
    mutex                   m_mtx;

    bool                    m_stop;   //停止服务器
};

//单例
ThreadPool* ThreadPool::instance = nullptr;

ThreadPool::ThreadPool(int thread_number, int max_request){
    assert(thread_number > 0 && max_request > 0);

    m_stop = false;
    m_max_request = max_request <= MAX_REQUEST? max_request : MAX_REQUEST;
    m_thread_number = thread_number < MAX_THREAD? thread_number: MAX_THREAD;

    for(int i = 0; i < thread_number; ++i){
        //创建线程，并且分离线程
        m_workers.emplace_back([this](){this->work();});
        m_workers[i].detach();
    }      
}

ThreadPool::~ThreadPool(){
    m_stop = true;
}

//消费者
void ThreadPool::work(){
   while(!m_stop){
        Request* request;
        //拿取请求
        {
            unique_lock<mutex> lock(this->m_mtx);
            //获取锁并等待，直到该请求队列中有请求到来
            this->m_cond.wait(lock, [this](){return !this->m_request_queue.empty();});

            //拿取请求
            request = m_request_queue.front();
            this->m_request_queue.pop_front();
        }
        //do request
        request->DoRequest();
    }
}




//生产者
bool ThreadPool::Append(Request* request){

//获取锁
    unique_lock<mutex> lock(m_mtx);
    if(m_request_queue.size() > m_max_request)
        return false;
    m_request_queue.push_back(request);
    
    m_cond.notify_one();

    return true; 
}










#endif