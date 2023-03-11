#ifndef PRIORITY_QUEUE_H_
#define PRIORITY_QUEUE_H_

//T类型必须有一个字段idx，并且T类型不能更改idx
//提供按元素删除
template<class T, class Comp, class Swap>
class PriorityQueue
{
private:
    void percolate_down(int i)
    {
        for(i *= 2; i <= this->size; i *= 2)
        {
            if(i+1 <= this->size && this->comp(heap[i+1], heap[i]))  //i和i+1中找到更符合条件的
                i++;
            
            if(this->comp(heap[i], heap[i/2])){  //下面的更符合条件，下溯
                this->swap(heap[i], heap[i/2]);
                heap[i]->idx = i;
                heap[i/2]->idx = i/2;
            }
            else
                break;
        }
    }  
    void percolate_up(int i){
        while(i > 1 && this->comp(heap[i], heap[i/2])){
        //heap[i]位置上溯
            this->swap(heap[i], heap[i/2]);
            heap[i]->idx = i;
            heap[i/2]->idx = i/2;
            i /= 2;
        }
    }

public:
    PriorityQueue(int capcity){
        this->capcity = capcity;
        this->size = 0;
        this->heap = new T*[capcity];
    }
    ~PriorityQueue(){
        if(this->heap != nullptr){
            delete [] this->heap;
        }
    }

    //只负责从heap中删除
    //key身上自带一个其在heap中的索引，
    //删除时，判断自己是否已经被删除/移动位置
    //若是则删除失败
    //input: key 对应元素
    //output: true 在对应i位置找到key并且删除
    //        false 在i位置没有删除
    bool erase_from_heap(T* val)
    {
        //若该val已经被delete掉
        if(!val){
            return false;
        }

        //该val已经被从优先队列中删除掉
        int i = val->idx;
        if(i > this->size || i < 1 || this->heap[i] != val){
            return false;
        }

        this->swap(heap[i], heap[this->size]);
        heap[i]->idx = i;
        heap[this->size]->idx = INT32_MIN;

        heap[this->size--] = nullptr;
        percolate_down(i);
        return true;
    }

    void push(T* val)
    {
        if(this->size == this->capcity){
            //扩容
            T** pre_heap = heap;
            this->capcity = this->capcity *this->expand;
            heap = new T*[this->capcity];
            for(int i = 1; i <= this->size; ++i){
                heap[i] = pre_heap[i];
            }
            delete [] pre_heap;
        }
        
        this->heap[++this->size] = val;
        val->idx = this->size;

        percolate_up(this->size);
    }

    T* top(){
        if(this->size < 1){
            return nullptr;
        }
        
        return this->heap[1];
    }
    bool empty(){
        return this->size == 0;
    }

    void pop()
    {
        if(this->size < 1){
            return;
        }
        else if(this->size == 1){
            this->heap[1]->idx = INT32_MIN;
            this->heap[this->size--] = nullptr;
            return;
        }

        this->swap(heap[1], heap[this->size]);
        heap[1]->idx = 1;
        heap[this->size]->idx = INT32_MIN;

        this->heap[this->size--] = nullptr;
        percolate_down(1);
    }

private:
    int const expand = 2;
    T** heap;
    int capcity;
    int size;

    Comp comp;
    Swap swap;
};

#endif