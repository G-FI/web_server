#include "../src/timer.h"
#include "../src/http_request.h"


#include<vector>
using namespace std;

int main(){
    sockaddr_in addr;
    vector<HttpRequest> users(10);


    TimerHeapThreadSafe timer_heap;
    timer_heap.AddTimer(&users[0], 1);
    timer_heap.AddTimer(&users[1], 2);
    timer_heap.AddTimer(&users[2], 3);
    timer_heap.AddTimer(&users[3], 4);
    timer_heap.AddTimer(&users[4], 5);
    timer_heap.AddTimer(&users[5], 6);
    timer_heap.AddTimer(&users[6], 7);
    timer_heap.AddTimer(&users[7], 8);
    timer_heap.AddTimer(&users[8], 9);
    timer_heap.AddTimer(&users[9], 10);

    while(true){
        timer_heap.Tick();
    }

    Request req;
    req.DoRequest();
    req.DoTimer();

}