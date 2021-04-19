#ifndef MY_EPOLL_H
#define MY_EPOLL_H

#include <sys/epoll.h>  //epoll_ctl()
#include <fcntl.h>      //fcntl()
#include <unistd.h>     //close()
#include <assert.h>     //close()
#include <vector>       
#include <errno.h>

using namespace std;
/*
------------------------------------------------------------------------------------
------------------------------------------------------------------------------------
epoll相关代码包括非阻塞模式、内核时间表注册时间、删除事件、重置EPOLLONSHOT时间四种

int epoll_create(int size)
创建一个指示epoll内核时间表的文件描述符，该描述符将用作其他epoll系统调用的第一个参数，size不起作用


int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
该函数用于操控内核事件表监控的文件描述符上的事件：注册事件、修改事件、删除事件。
epfd：为epoll_create的句柄，由epoll_create生成的epoll专用的文件描述符。
op：表示动作，用3个宏来表示
    1、EPOLL_CTL_ADD(注册新的fd到epfd)
    2、EPOLL_CTL_MOD(修改已经注册的fd的监听事件)
    3、EPOLL_CTL_DEL(从epfd删除一个fd)
event：告诉内核需要监听的事件，指向epoll_event的指针
struct epoll_event{
    _unit32_t events; //epoll events
    epoll_data_t data;  //User data variable 
}

events描述事件类型，其中epoll事件类型有以下几种
    1、EPOLLIN：表示对应的文件描述符可以读。（包括对端SOCKET正常关闭）
    2、EPOLLOUT：表示对应的文件描述符可以写
    3、EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）
    4、EPOLLERR：表示对应的文件描述符发生错误
    5、EPOLLHUP：表示对应的文件描述符被挂断；
    6、EPOLLET：将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)而言的
    7、EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里

int epoll_wait(int epfd , struct epoll_event *events , int maxEvents ,int timeoout);
    等待事件的产生，类似于select()调用。参数events用来从内核得到事件的集合，maxevents告知内核这个events有多大，
maxEvents的值不能大于创建epoll_create()时的size，参数timeout是超时时间(0会立即返回，-1表示不确定，也可能永久阻塞)。
    该函数返回需要处理的事件数目，返回0表示已超时。
返回的事件集合在events数组中，数组中实际存放的成员个数是函数的返回值，返回0表示已经超时。

    epfd：由epoll_create生成的epoll文件描述符。
    epoll_event:用于回传待处理事件的事件数。
    maxEvents：每次能处理的事件数。
    timeout：等待I/O事件发生的超时值，-1相当于阻塞，0相当于非阻塞。
    epoll_wait运行原理：等待注册的时间在epfd上的socket fd的事件的发生，如果发生则将产生的socket fd和事件类型放入到events数组中。
并且将注册在epfd上的socket fd的事件类型清空。

*/

class my_epoll{
public:
    explicit my_epoll(int maxEvent = 1024);
    ~my_epoll();

private:
    int epoll_fd ; 
    vector<struct epoll_event > vec_events;

public:
    bool AddFd(int fd , uint32_t events);
    bool ModFd(int fa , uint32_t events);
    bool DelFd(int fd);
    int Wait(int TimeoutMs = -1);
    int GetEventFd(size_t i) const;
    uint32_t GetEvents(size_t i) const;
};

my_epoll::my_epoll(int maxEvent):epoll_fd(epoll_create(512)),vec_events(maxEvent){
    assert(epoll_fd >= 0 && (vec_events.size() > 0) );
}

my_epoll:~my_epoll(){
    close(epoll_fd);
}

bool my_epoll::AddFd(int fd , uint32_t events){
    if(fd < 0) return false;

    epoll_event ep_ev = {0};
    ep_ev.data.fd = fd;
    ep_ev.events = events;
    return epoll_ctl(epoll_fd , EPOLL_CTL_ADD , fd ,&ep_ev) == 0;
}

bool my_epoll::ModFd(int fd , uint32_t events){
    if(fd < 0) return false;

    epoll_event ep_ev = {0};
    ep_ev.data.fd = fd;
    ep_ev.events = events;
    return epoll_ctl(epoll_fd , EPOLL_CTL_MOD , fd ,&ep_ev) == 0;
}

bool my_epoll::DelFd(int fd){
    if(fd < 0) return false;

    epoll_event ep_ev = {0};
    return epoll_ctl(epoll_fd , EPOLL_CTL_DEL , fd ,&ev) == 0;
}

int my_epoll::Wait(int TimeoutMs){
    return epoll_wait(epoll_fd , &vec_events[0] , static_cast<int>(vec_events.size()),TimeoutMs);
}

int my_epoll::GetEventFd(size_t i) const {
    assert( i >= 0 && i < vec_events.size() );
    return vec_events[i].data.fd;
}

uint32_t my_epoll::GetEvents(size_t i) const{
    assert( i >= 0 && i < vec_events.size() );
    return vec_events[i].events;
}

#endif /*MY_EPOLL_H*/