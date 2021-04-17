#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>
#include <map>
#include <string.h>
#include <stdlib.h> 

#include "locker.h"
#include "mysql_connection_pool.h"

//#define connfdET //边缘触发非阻塞
#define connfdLT //水平触发阻塞

//#define listenfdET //边缘出发非阻塞
#define listenfdLT //水平触发阻塞

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

const char *doc_root = "/";

class http_connection{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD{
        GET = 0,
        POST ,
        HEAD ,
        PUT ,
        DELETE ,
        TRACE ,
        OPTIONS ,
        CONNECT ,
        PATH
    };
    enum CHECK_STATE{
        CHECK_STATE_REQUESLINE = 0,
        CHECK_STATE_HEADER ,
        CHECK_STATE_CONTENT,
    };
    enum HTTP_CODE{
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUESET,
        NO_REQUESET,
        FORBIDDEN_REQUESET,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    enum LINE_STATUS{
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };
private:
    int sockfd;
    sockaddr_in address;
    char read_buf[READ_BUFFER_SIZE];
    int read_idx;
    int checked_idx;
    int start_line;
    char write_buf[Write_BUFFER_SIZE];
    int write_idx;
    CHECK_STATE check_state;
    METHOD method;
    char real_file[FILENAME_LEN];
    char *url;
    char *version;
    char *host;
    int content_length;
    bool linger;
    char *file_address;
    struct stat file_stat;
    struct iovec iv[2];
    int iv_count;
    int cgi;//是否启用post
    char *str; //存储请求头信息
    int bytes_to_send;
    int bytes_have_send;

public:
    static int epoll_fd;
    static int user_count;
    MYSQL *mysql;

public:
    http_connection() {};
    ~http_connection() {};

public:
    void init(int sockfd , const sockaddr_in &addr);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address() {return &address;}
    void initmysql_result(mysql_connection_pool *connPool);

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() {return read_buf + start_line;};
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *formate, ...);
    bool add_content(const char *content);
    bool add_status_line(int status,const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
}

map<string , string > users;
locker lock;

void http_connection::initmysql_result(mysql_connection_pool *connPool){
    //从连接池中获取一个连接
    MYSQL *new_mysql = NULL;
    connectionINIT mysqlcon(&new_mysql , connPool);

    //从user表中检索username，passwd数据，浏览器端输入
    if(mysql_query(new_mysql , "select username,passwd from user")){

    }
    //获取完整的数据
    MYSQL_RES *result = mysql_store_result(new_mysql);
    // 返回结果集合中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    while(MYSQL_ROW row = mysql_fetch_row(result)){
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

//对文件描述符设置为非阻塞
int setnonblocking(int fd){
    int old_option = fctntl(fd , F_GETFL);
    //O_NONBLOCK：非阻塞I/O
    int new_option = old_option | O_NONBLOCK;
    //F_SETFL：设置给arg描述符状态标志,可以更改的几个标志是：O_APPEND， O_NONBLOCK，O_SYNC和O_ASYNC。
    fcntl(fd , F_SETFL , new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd , int fd , bool one_shot){
    epoll_event event ;
    event.data.fd = fd;

#ifdef connfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

#ifdef listenfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef listenfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfa , EPOLL_CTL_ADD , fd , &event);
    setnonblocking(fd);
}

//从内核时间表删除描述符
void removefd(int epollfd , int fd){
    epoll_ctl(epollfd , EPOLL_CTL_DEL , fd , 0);
    close(fd);
}

//将时间重置为EPOLLONESHOT
void modfd(int epollfd , int fd , int ev){
    epoll_event event;
    event.data.fd = fd;
#ifdef connfdET
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif

    epoll_ctl(epollfd , EPOLL_CTL_MOD , fd , &event);
}

int http_connection::user_count = 0;
int http_connection::epollfd = -1;

//关闭连接，关闭一个连接，客户总量减一
void http_connection::close_conn(bool real_close){
    if(real_close && (sockfd != -1)){
        removefd(epollfd , sockfd);
        sockfd = -1;
        --user_count;
    }
}

// 初始化连接，外部调用初始化套接字地址
void http_connection::init(int sockfd , const sockaddr_in &addr){
    this->sockfd = sockfd;
    this->address = addr;
    addfd(epollfd , sockfd , true);
    ++user_count;
    init();
}

//初始化新接受的连接
//check_state 默认为分析请求状态
void http_connection::init(){
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    check_state = CHECK_STATE_REQUESLINE ;
    linger = false;
    method = GET;
    url = 0;
    version = 0;
    content_length = 0;
    host = 0;
    start_line = 0;
    checked_idx = 0;
    read_idx = 0;
    cgi = 0;
    memset(read_buf , '\0' , READ_BUFFER_SIZE);
    memset(write_buf , '\0' , WRITE_BUFFER_SIZE);
    memset(real_file , '\0' , FILENAME_LEN);
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK，LINE_BAD，LINE_OPEN
http_connection::LINE_STATS http_connection::parse_line(){
    char temp;
    for( ; checked_idx < read_idx ; ++checked_idx){
        temp = read_buf[checked_idx];
        if(temp == '\r'){

            if( (checked_idx+1) == read_idx){
                return LINE_OPEN;
            }else if( read_buf[checked_idx+1] == '\n' ){
                read_buf[checked_idx++] = '\0';
                read_buf[checked_idx++] = '\0';
                return LINE_OK;
            }

            return LINE_BAD;
        }else if(temp == '\n'){

            if(checked_idx > 1 && read_buf[checked_idx - 1] == '\r'){
                read_buf[checked_idx - 1 ] = '\0';
                read_buf[checked_idx++] = '\0';
                return LINE_OK;
            }

            return LINE_BAD;

        }
    }

    return LINE_OPEN;
}

//循环读取客户数据，知道无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool http_connection::read_once(){
    if(read_idx >= READ_BUFFER_SIZE){
        return false;
    }
    
    int bytes_read = 0;
#ifdef connfdLT
    bytes_read = recv(sockfd , read_buf + read_idx , READ_BUFFER_SIZE - read_idx , 0);
    read_idx += bytes_read;

    if(bytes_read <= 0){
        return false;
    }
    return true;
#endif

#ifdef connfdET
    while(true){
        bytes_read = recv(sockfd,read_buf+read_idx,READ_BUFFER_SIZE-read_idx,0);
        if(bytes_read == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            return false;
        }else if(bytes_read == 0){
            return false;
        }
        read_idx += bytes_read ; 
    }
    return true;
#endif
}

//解析http请求行，获取请求方法，目标url以及http版本号
http_connection::HTTP_CODE http_connection::parse_request_line(char **text){
    url = strpbrk(text , " \t");
    if(!url) return BAD_REQUEST;

    *(this->url)++ = '\0';
    char *new_method = text;
    if(strcasecmp(new_method , "GET") == 0) {
        this->method = GET;
    }else if(strcasecmp(new_method , "POST") == 0) {
        this->method = POST;
        cgi=1;
    }else return BAD_REQUEST;
    this->url += strspn(this->url , "\t");
    this->version = strpbrk(this->url , " \t");
    if(!(this->version)) return BAD_REQUEST;
    *(this->version)++ = '\0';
    (this->version) = strpbrk(this->version , " \t");

    if(strcasecmp(this->version , "HTTP/1.1") != 0) return BAD_REQUEST;
    if(strncasecmp(this->url , "http://",7) == 0){
        this->url +=7;
        this->url = strchr(this->url , '/');
    }

    if(!(this->url) || (this->url[0] != '/')) return BAD_REQUEST;

    //当url为/时，显示判断界面
    if(strlen(this->url) == 1) strcat(this->url , "judge.html");
    check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析http请求的一个头部信息
http_connection::HTTP_CODE http_connection::parse_headers(char *text){
    if(text[0] == '\0'){
        if(content_length != 0){
            check_state = CHECK_STATE_CONTENT;
            return NO_REQUESET;
        }
    }else if( strncasecmp(text , "Connection:" , 11) == 0 ){
        text += 11;
        text += strspn(text , " \t");
        if( strncasecmp(text , "keep-alive") == 0 ) {
            linger = true;
        }
    }else if( strncasecmp(text , "Content-length:" , 15) == 0 ){
        text += 15;
        text += strspn(text , " \t");
        content_length = atol(text);
    }else if( strncasecmp(text , "Host:" , 5) == 0 ){
        text += 5;
        text += strspn(text , " \t");
        host = text;
    }else {

    }
    return NO_REQUEST;
}

//判断http请求是否被完整读入
http_connection::HTTP_CODE http_connection::parse_content(char *text){
    if(read_idx >= (content_length + checked_idx)){
        text[content_length ] = '\0';
        str = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_connection::HTTP_CODE http_connection::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    
    while( (check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || 
    ( (line_status = parse_line() )== LINE_OK) ){
        text = get_line();
        start_line = checked_idx;

        switch (check_state))
        {
        case CHECK_STATE_REQUESLINE:{
            ret = parse_request_line(text);
            if(ret == BAD_REQUEST)
                return BAD_REQUESET;
            break;
        }
        case CHECK_STATE_HEADER:{
            ret = parse_headers(text);
            if(ret == BAD_REQUESET ):
                return BAD_REQUESET;
            else if( ret == GET_REQUEST){
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:{
            ret = parse_content(text);
            if(ret == GET_REQUEST)
                return do_request;
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

http_connection::HTTP_CODE http_connection::do_request(){
    strcpy(real_file , doc_root);
}


#endif HTTP_CONNECTION_H