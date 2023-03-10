#ifndef TIMER_H_
#define TIMER_H_

#include <iostream>
#include <netinet/in.h>
#include <time.h>
#include <queue>
#include <vector>

using std::cout;
using std::endl;
using std::priority_queue;
using std::vector;

class UserData{
public:
    UserData(int sockfd_, sockaddr_in addr_):sockfd(sockfd_), addr(addr_){}

    int sockfd;
    sockaddr_in addr;

    void DoTimer(){
        cout<<"client "<<sockfd<<" timer done"<<endl;
    }
};

class Timer{
public:
    Timer(UserData* user_data_, int delay){
        expired_time = time(nullptr) + delay;
        user_data = user_data_;
    }
    int expired_time;
    UserData* user_data;
};
class TimerComp{
public:
    bool operator()(const Timer* lhs, const Timer* rhs) const{
        return lhs->expired_time > rhs->expired_time;
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
    void AddTimer(UserData* user_data, int delay){
        Timer* tm = new Timer(user_data, delay);
        timer_heap->push(tm);
    }

    //tick函数，处理到期定时器
    void Tick(){
        updateTime();
        while(!timer_heap->empty()){
            
            Timer* next_timer = timer_heap->top();
            if(cur_time >= next_timer->expired_time){
                next_timer->user_data->DoTimer();
                timer_heap->pop();
                delete next_timer;
            }
            else{
                break;
            }
        }
    }

    int size(){
        return timer_heap->size();
    }

    ~TimerHeap(){
        while(!timer_heap->empty()){
            Timer* tmp = timer_heap->top();
            delete tmp;
            timer_heap->pop();
        }

        delete timer_heap;
    }

private:
    int cur_time;
    priority_queue<Timer*, vector<Timer*>, TimerComp> *timer_heap; 
};






#endif 