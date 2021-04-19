#ifndef MYSQL_CONNECTION_H
#define MYSQL_CONNECTION_H

// linux下，mysql提供的C API连接数据库
#include <mysql/mysql.h>
#include <string>
#include <list>
#include <pthread.h>
#include "locker.h"

using namespace std;

class mysql_connection_pool{
public:
    static mysql_connection_pool *GetInstance();

private:
    string Url;                           //主机地址
    string Port;                          //数据库端口号
    string User;                          //用户名
    string password;                      //密码
    string DatabaseName;                  //数据库名

    int MaxConnection;                    //最大连接数
    int CurConnection;                    //当前线程池中已连接数
    int FreeConnection;                   //当前线程池中空闲连接数

    locker lock;                            //锁保证连接池的线程安全
    list<MYSQL *> connectList               //连接池包含所有可用的连接池
    sem reserve                             //信号量

public:
    MYSQL *GetConnection();                 //获取数据库的连接
    int GetFreeConnection();                //获取空闲连接数
    bool ReleaseConnection(MYSQL *connect); //释放响应的连接
    void Destroypool();                     //释放所有连接，用于析构

    void init(string url,int port,string user,string password,string databasename,int Maxconect);

    mysql_connection_pool();                //构造
    ~mysql_connection_pool();               //析构

};

// 初始化时连接数和空闲数均为0
mysql_connection_pool::connection_pool(){
    this->CurConnection = 0;
    this->FreeConnection = 0;
}

mysql_connection_pool *mysql_connection_pool::GetInstance(){
    static mysql_connection_pool connectPool;
    return &connectPool;
}

void mysql_connection_pool::init(string url,int port,string user,string password,
                                string databasename,int Maxconect = 10){
    assert(Maxconect > 0);
    this->Url = url ;
    this->Port = port ;
    this->User = user ;
    this->Password = password;
    this->DatabaseName = databasename;

    lock.lock();

    for(int i = 0 ; i < Maxconect ; ++i ){
        MYSQL *conn = nullptr;
        //初始化连接
        conn = mysql_init(conn);

        if(conn == nullptr){
            exit(1);
        }

        //建立一个到mysql数据库的连接
        //MYSQL *mysql_real_connect(MYSQL *mysql, const char *host, const char *user, const char *passwd, const char *db, unsigned int port, const char *unix_socket, unsigned long client_flag)
        conn = mysql_real_connect(conn,url.c_str().user.c_str(),password.c_str(),databasename.c_str(),port,NULL,0);
        
        if(!conn){
            exit(1);
        }

        connectList.emplace_back(conn);
        ++FreeConnection;
    }

    reserve = sem(FreeConnection);
    MaxConnection = FreeConnection;

    lock.unlock();

}

//有请求时，从数据库连接池中返回一个可用的连接，更新已使用和空闲的连接数
MYSQL *mysql_connection_pool::GetConnection(){
    MYSQL *conn = nullptr;
    if(connectList.size() == 0) {
        // 日志查错 ~~
        return nullptr;
    }
    
    reserve.wait();

    lock.lock();
    
    conn = connectList.front();
    connectList.pop_back();
    --m_FreeConnection;
    ++m_CurConnection;
    
    lock.unlock();

    return conn;
}

//释放当前使用的连接
bool mysql_connection_poll::ReleaseConnection(MYSQL *conn){
    if(conn == nullptr) return false;

    lock.lock();

    connectList.emplace_back(conn);
    ++m_FreeConnection;
    --m_CurConnection;

    lock.unlock();

    reserve.post();

    return true;
}

//销毁数据库连接池
void mysql_connection_pool::Destroypool(){

    lock.lock();

    if(connectList.size() > 0){

        list<MYSQL *>::iterator it;
        for(it = connectList.begin() ; it != connectList.end() ;++it){
            MYSQL *conn = *it;
            //关闭连接
            mysql_close(conn);
        }
        CurConnection = 0;
        FreeConnection = 0;
        connectList.clear();

    }

    lock.unlock();
}

//获取当前空闲的连接数
int mysql_connection_pool::GetFreeConnection(){
    lock.lock();
    return this->FreeConnection;
    lock.unlock();
}

//释放连接池
mysql_connection_pool::~mysql_connection_pool(){
    Destroypool();
}

#endif /*MYSQL_CONNECTION_H*/