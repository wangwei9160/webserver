#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "locker.h"
#include "mysql_connection_pool.h"

template<class T>
class threadpool{
private:
    int thread_number;                      //线程中的线程数量
    int max_requests;                       //请求队列中允许的最大连接数，最多允许等待处理的线程数量
    pthread_t *threads;                     //描述线程池的数组，大小为thread_number
    std::list<T *> workqueue;               //请求队列
    locker queuelocker;                     //保护请求队列的锁
    sem queuestat;                          //是否有任务需要处理的信号量
    mysql_connection_pool *connectPool ;    //数据库
    int actor_model;                        //模型

private :
    /* 工作线程运行的函数，它不断从工作队列中取出任务并执行 */
    static void *worker(void *arg);
    void run();

public:
    threadpool(int actor_model ,mysql_connection_pool *connection_pool,
                int thread_number = 8,int max_requests = 10000);
    ~threadpool();
    bool append(T *request , int state);
    bool append_p(T *request);

}

template <class T>
threadpool<T>::~threadpool(){
    operator delete[] threads;
}

/*
向请求队列添加任务，通过互斥锁保证线程安全，添加完成后通过信号量提醒有任务要处理，最后保证线程同步。
*/

template <class T>
bool threadpool<T>::append(T *request ,int state){

    queuelocker.lock();

    if(workqueue.size() >= max_requests){
        queuelocker.unlock();
        return false;
    }
    request->state = state;
    workqueue.push_back(request);
    queuelocker.unlock();

    queuestat.post();
    return true;
}

template <class T>
bool threadpool<T>::append_p(T *request){
    queuelocker.lock();
    if( workqueue.size() >= max_requests ){
        queuelocker.unlock();
        return false;
    }
    workqueue.push_back(request);
    queuelocker.unlock();
    queuestat.post();
    return true;
}

/*
将参数强转为线程池类，调用成员方法
*/

template <class T>
void *threadpool<T>::worker(void *arg){
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

/*
工作线程从请求队列中取出某个任务进行处理
*/

template <class T>
void threadpool<T>::run(){
    while(true){
        queuestat.wait();               //append通过queuestat来通知有信号

        queuelocker.lock();             //线程锁
        if(workqueue.empty()){  
            queuelocker.unlock();
            continue;
        }

        /*从请求队列取出第一个任务，将任务从请求队列删除*/
        T *request = workqueue.front();
        workqueue.pop_back();

        queuelocker.unlock();
        if(!request) continue;

        //从连接池中取出一个数据库连接
        if( actor_model == 1){
            if(request->state == 0){
                if(  request->read_once() ){
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql , connectpool);
                    request->process();
                }else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }else {
                if(request->write()){
                    request->improv = 1;
                }else {
                    request->improv = 1;
                    request->timer_falg = 1;
                }
            }
        }else {
            connectionRAII mysqlcon(*request->mysql , connectPool);
            request->process();
        }
    }
}

#endif /*THREADPOOL_H*/