#include "../threadpool.h"
#include "../request.h"


#include <iostream>
#include <thread>
#include <chrono>


using namespace std;

class CustomerRequest: public Request{
public:
    virtual void Process() override{
        this_thread::sleep_for(chrono::seconds(2));
        cout<<"I'm processing in thread: "<<this_thread::get_id()<<endl;
    }
};

void wrok(Request* request){
    request->Process();
}


int main(){
    ThreadPool* pool = ThreadPool::GetInstance();

    vector<CustomerRequest> requests(20);

    for(auto &r: requests){
        pool->Append(&r);
    }




    this_thread::sleep_for(chrono::seconds(100));
}