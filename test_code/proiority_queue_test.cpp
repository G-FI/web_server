#include "../src/priority_queue.h"
#include "../src/timer.h"
#include <iostream>

using namespace std;

int main(){
    PriorityQueue<Timer, TimerComp, TimerSwap> timer_heap(20);


    vector<Timer*> timers ={
        new Timer(nullptr, 10),
        new Timer(nullptr, 40),
        new Timer(nullptr, 70),
        new Timer(nullptr, 120),
        new Timer(nullptr, 20),
        new Timer(nullptr, 1),
        new Timer(nullptr, 8),
        new Timer(nullptr, 7),
        new Timer(nullptr, 100),
        new Timer(nullptr, 89),
        new Timer(nullptr, 60),
        new Timer(nullptr, 10),
    };
    for(int i = 0; i < timers.size(); ++i){
        timer_heap.push(timers[i]);
    }

    for(int i = 0; i < timers.size(); ++i){
        cout<< timers[i]->idx <<"   "<<timers[i]->expired_time<<endl;
    }

    

    //按元素删除
    for(int i = 0; i< timers.size(); ++i){
        timer_heap.erase_from_heap(timers[i]);
    }

    for(int i = 0; i < timers.size(); ++i){
        cout<< timers[i]->idx <<"   "<<timers[i]->expired_time<<endl;
    }

    while(!timer_heap.empty()){
        Timer* top =  timer_heap.top();
        timer_heap.pop();
        cout<<top->expired_time<<" "<<top->idx<<endl;
        delete top;
    }
    
}