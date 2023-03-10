#ifndef REQUEST_H_
#define REQUEST_H_

#include <iostream>

using std::cout;
using std::endl;

class Request{
public:

    virtual void DoRequest(){
        cout<<"I'm doing request"<<endl;
    }
    virtual void DoTimer(){
        cout<<"I'm doing Timer"<<endl;
    }

};

#endif