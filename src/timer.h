#ifndef TIMER_H_
#define TIMER_H_

#include <iostream>
#include <netinet/in.h>
#include <time.h>
#include <queue>
#include <vector>
#include <mutex>
#include "request.h"
#include "priority_queue.h"

using std::cout;
using std::endl;
using std::priority_queue;
using std::vector;

using std::mutex;
using std::lock_guard;
using std::unique_lock;

class Timer {
public:
    Timer(){}
    Timer(Request *request, int delay){
        this->expired_time = time(nullptr) + delay;
        this->request = request;
        this->idx = 0;
    }
    void Reset(Request* request, int delay){
        this->expired_time = time(nullptr) + delay;
        this->request = request;
        this->idx = 0;
    }
    
    int expired_time;
    int idx;            //Timer在时间堆中的索引，便于之后根据Timer进行删除自己
    Request* request;
};


class TimerComp{
public:
    bool operator()(const Timer* lhs, const Timer* rhs) const{
        return lhs->expired_time < rhs->expired_time;
    }
};

class TimerSwap{
public:
    void operator()(Timer*& lhs,Timer*& rhs){
        Timer* tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }
};

class TimerHeap{
    void updateTime(){
        cur_time = time(nullptr);
    }


public:
    //创建时间堆
    TimerHeap(){
        timer_heap = new priority_queue<Timer*, vector<Timer*>, TimerComp>(TimerComp());
    }

    //时间堆中添加定时器
    /***
     * delay: 定时时间
    */
    void AddTimer(Request* request, int delay){
        Timer* tm = new Timer(request, delay);
        timer_heap->push(tm);
    }

    //tick函数，处理到期定时器
    void Tick(){

        updateTime();
        while(!timer_heap->empty()){
            
            Timer* next_timer = timer_heap->top();
            if(cur_time >= next_timer->expired_time){
                next_timer->request->DoTimer();
                timer_heap->pop();
                delete next_timer;
            }
            else{
                break;
            }
        }
    }

    int Size(){
        return timer_heap->size();
    }

    ~TimerHeap(){
        deleteHeap();
    }

protected:
    void deleteHeap(){
        while(!timer_heap->empty()){
            Timer* tmp = timer_heap->top();
            delete tmp;
            timer_heap->pop();
        }

        delete timer_heap;
    }
    int cur_time;
    priority_queue<Timer*, vector<Timer*>, TimerComp> *timer_heap; 
};


class TimerHeapThreadSafe{
private:
    void updateTime(){
        cur_time = time(nullptr);
    }
    TimerHeapThreadSafe(int capcity = 100):timer_heap(capcity){}
public:
    static TimerHeapThreadSafe* GetInstance(){
        if(instance == nullptr){
           instance = new TimerHeapThreadSafe();
        }
        return instance;
        //return nullptr;
    }
    void AddTimer(Request* request, int delay){
        lock_guard<mutex> lk(this->mtx);
        Timer *timer = new Timer(request, delay);
        timer_heap.push(timer);
    }
    //input: timer为堆区创建, 由该TimerHeap来负责delete
    void AddTimer(Timer* timer){
        lock_guard<mutex> lk(this->mtx);
        timer_heap.push(timer);
    }

    //返回调用时刻的时间
    int Tick(){
        unique_lock<mutex> lk(this->mtx);
        updateTime();
        int ret = this->cur_time;
        while(!timer_heap.empty()){
            Timer* next_timer = timer_heap.top();
            if(this->cur_time >= next_timer->expired_time){
                timer_heap.pop();
                //释放锁，处理定时事件，此时该请求一定不会被处理
                // this->mtx.unlock();
                lk.unlock();
                
                next_timer->request->DoTimer();

                //获取锁继续处理定时事件
                // this->mtx.lock();
                lk.lock();
            }
            else{
               break; 
            }
        }
        lk.unlock();
        return ret;
    }  

    //根据元素删除
    //output: true 表示该定时器没有触发，并且正确地从时间堆中删除
    //        false 定时器已被触发
    bool RemoveTimer(Timer* timer){
        lock_guard<mutex> lk(this->mtx);
        bool ret =  timer_heap.erase_from_heap(timer);
        return ret;
    }

    //得到**[start, next_timer]**下一个最早到期时间还需要的时间
    //返回0表示队列中没有
    int  MinTime(int start){

        lock_guard<mutex> lk(this->mtx);
        
        Timer* next_timer = this->timer_heap.top();
        if(next_timer == nullptr){
            return 0;
        }
        else{
            return  next_timer->expired_time - start;
        }
    }
    ~TimerHeapThreadSafe(){
        lock_guard<mutex> lk(this->mtx);        
        while(!timer_heap.empty()){
            Timer * tmp = timer_heap.top();
            timer_heap.pop();
            delete tmp;
        }
    }
private:
    static TimerHeapThreadSafe* instance;
    int cur_time;
    mutex mtx;
    PriorityQueue<Timer, TimerComp, TimerSwap>  timer_heap;
};

#endif 