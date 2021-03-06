#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

/*
get : 向服务器获取数据
post ： 向服务器发数据
*/

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
    static const int FILENAME_LEN = 200;            // 设置读取文件的名称 real_file 的大小
    static const int READ_BUFFER_SIZE = 2048;       // 设置读缓冲区 read_buf 的大小
    static const int WRITE_BUFFER_SIZE = 1024;      // 设置写缓冲区 write_buf 的大小
    // 报文的请求方法，本项目中只会使用到GET和POST
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
    /*
    主状态机的状态
    1、CHECK_STATE_REQUESLINE    解析请求行
    2、CHECK_STATE_HEADER        解析请求头
    3、CHECK_STATE_CONTENT       解析消息体，仅用于解析post请求
    */
    enum CHECK_STATE{
        CHECK_STATE_REQUESLINE = 0,
        CHECK_STATE_HEADER ,
        CHECK_STATE_CONTENT,
    };
    /*
    从状态机的状态
    1、LINE_OK      完整读取一行
    2、LINE_BAD     报文语法有误
    3、LINE_OPEN    读取的行不完整
    */
    enum LINE_STATUS{
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };
    /*
    报文解析的结构
    1、NO_REQUEST       请求不完整，需要继续读取请求报文数据
    2、GET_REQUEST      获得了完整的HTTP请求
    3、BAD_REQUESET     HTTP请求报文有语法错误
    4、INTERNAL_ERROR   服务器内部错误，该结果在主状态机逻辑switch的default下，一般不会触发
    */
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
    
private:
    int sockfd;
    sockaddr_in address;

    char read_buf[READ_BUFFER_SIZE];                    //存储读取的请求报文数据
    int read_idx;                                       //缓冲区read_buf中数据的最后一个字节的下一个位置
    int checked_idx;                                    //read_buf读取的位置
    int start_line;                                     //read_buf已经解析的字符的个数

    char write_buf[Write_BUFFER_SIZE];                  //存储发出的响应报文数据
    int write_idx;                                      //write_buf的长度

    CHECK_STATE check_state;                            //主状态机的状态
    METHOD method;                                      //请求方法

    //解析请求报文中对应的6个变量
    char real_file[FILENAME_LEN];                       //存储读取文件的名字
    char *url;
    char *version;
    char *host;
    int content_length;
    bool linger;

    char *file_address;                                 //读取服务器上的文件地址
    struct stat file_stat;                          
    struct iovec iv[2];                                 //io向量机制iovec
    int iv_count;                                   
    int cgi;                                            //是否启用的post
    char *str;                                          //存储请求头信息
    int bytes_to_send;                                  //剩余发送的字节数
    int bytes_have_send;                                //已经发送的字节数

public:
    static int epoll_fd;
    static int user_count;
    MYSQL *mysql;

public:
    http_connection() {};
    ~http_connection() {};

public:
    void init(int sockfd , const sockaddr_in &addr);        //对私有变量进行初始化
    void close_conn(bool real_close = true);
    void process();
    bool read_once();                                       //读取浏览器端发送来的请求报文，直到无数据可读或对方关闭连接，读取到read_buf中，同时更新read_idx
    bool write();
    sockaddr_in *get_address() {return &address;}
    void initmysql_result(mysql_connection_pool *connPool);

private:
    void init();                                            
    HTTP_CODE process_read();                               //从read_buf读取，并处理请求报文
    bool process_write(HTTP_CODE ret);                      //向write_buf写入响应报文数据
    HTTP_CODE parse_request_line(char *text);               //主状态机解析报文中的请求行数据
    HTTP_CODE parse_headers(char *text);                    //主状态机解析报文中的请求头数据
    HTTP_CODE parse_content(char *text);                    //主状态机解析报文中的请求内容
    HTTP_CODE do_request();                                 //生成响应报文


    char *get_line() {return read_buf + start_line;};       //用于将指针向后偏移，指向未处理的字符
    LINE_STATUS parse_line();                               //从状态机读取一行，分析是请求报文的那一部分

    void unmap();

    // 根据响应报文的格式，生成对应8个部分，均由do_request调用
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
    connectionRAII mysqlcon(&new_mysql , connPool);

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
    bytes_read = recv(sockfd,read_buf+read_idx,READ_BUFFER_SIZE-read_idx,0);
    read_idx += bytes_read;

    if(bytes_read <= 0){
        return false;
    }
    return true;
#endif

#ifdef connfdET
    while(true){
        // 从套接字接受数据，存储在read_buf缓冲区
        bytes_read = recv(sockfd,read_buf+read_idx,READ_BUFFER_SIZE-read_idx,0);
        if(bytes_read == -1){
            //非阻塞ET模式下，需要一次性将数据读完
            if(errno == EAGAIN || errno == EWOULDBLOCK) 
                break;
            return false;
        }else if(bytes_read == 0){
            return false;
        }
        //修改read_idx的读取字节数
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
    if( read_idx >= (content_length + checked_idx) ){
        text[content_length ] = '\0';

        // post 请求中最后是输入的用户名和密码
        str = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 读报文
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
    int len = strlen(doc_root);
    const char *p = strtchr(url , '/');

    //处理cgi
    if(cgi == 1 && (*(p+1) == '2' || *(p+1) == '3')){
        char flag = url[1];
        char *url_real = (char *)malloc(sizeof(char)*200);
        strcpy(url_real , "/");
        strcat(real_file + len , url_real , FILENAME_LEN - len - 1);

        //用户名密码提取出来
        char name[100] , password[100];
        int i;
        for(i = 5 ; str[i] != '&' ; ++i){
            name[i-5] = str[i];
        }
        name[i-5] = '\0';

        int j = 0 ;
        for(i = i+10 ; str[i] != '\0' ; ++i , ++j){
            password[j] = str[i];
        }
        password[j] = '\0';

        if( *(p+1) == '3' ){
            // 如果是注册的，先检测数据库中是否有重名
            // 没有重名的则进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert , "insert into user(username,passwd) values(");
            strcat(sql_insert , "'");
            strcat(sql_insert,name);
            strcat(sql_insert , "','");
            strcat(sql_insert,password);
            strcat(sql_insert , "')");

            if(user.find(name) == user.end()){
                lock.lock();
                int res = mysql_query(mysql , sql_insert);
                users.insert(pair<string ,string>(name,password));
                lock.unlock();

                if(!res) strcpy(url , "/login.html");
                else strcpy(url , "/registerError.html");
            }else {
                strcpy(url , "/registerError.html");
            }
        }else if( *(p+1) == '2' ){
            // 如果是登录，直接判断
            // 若浏览器端输入的用户名和密码在表中可以找到，返回1，否则返回0
            if(user.find(name) != user.end() && users[name] == password ){
                strcpy(url , "/welcome.html");
            }else {
                strcpy(url , "/loginError.html");
            }
        }
    }

    if( *(p+1) == '0' ){
        char *url_real = (char *)malloc(sizeof(char) * 200);

        strcpy(url_real , "/register.html");
        strcpy(real_file+len,url_real,strlen(url_real));
        
        free(url_real);
    }else if( *(p+1) == '1' ){
        char *url_real = (char *)malloc(sizeof(char) * 200);

        strcpy(url_real , "/login.html");
        strcpy(real_file+len,url_real,strlen(url_real));
        
        free(url_real);
    }else if( *(p+1)  == '5' ){
        char *url_real = (char *)malloc(sizeof(char) * 200);

        strcpy(url_real , "/picture.html");
        strcpy(real_file+len,url_real,strlen(url_real));
        
        free(url_real);
    }else if( *(p+1)  == '6' ){
        char *url_real = (char *)malloc(sizeof(char) * 200);

        strcpy(url_real , "/video.html");
        strcpy(real_file+len,url_real,strlen(url_real));
        
        free(url_real);
    }else if( *(p+1)  == '6' ){
        char *url_real = (char *)malloc(sizeof(char) * 200);

        strcpy(url_real , "/fans.html");
        strcpy(real_file+len,url_real,strlen(url_real));
        
        free(url_real);
    }else {
        strncmp(real_file+len , url , FILENAME_LEN-len-1);
    }

    if(stat(real_file , file_stat) < 0){
        return NO_REQUEST;
    }

    if( !(file_stat.st_mode & S_IROTH) ) {
        return FORBIDDEN_REQUESET;
    }

    if( S_ISDIR(file_stat.st_mode) )
        return BAD_REQUESET;

    int fd = open("real_file , O_RDONLY");
    file_address = (char *)mmap(0,file_stat.st_size , PROT_REAED, MAP_PRIVATE, fd,0);
    close(fd);
    return FILE_REQUEST;
}

void http_connection::unmap(){
    if(file_address){
        munmap(file_address , file_stat.st_size);
        file_address = 0;
    }
}

bool http_connection::write(){
    int temp = 0;

    if(bytes_to_send == 0){
        modfd(epollfd , sockfd , EPOLLIN);
        init();
        return true;
    }

    while(1){
        temp = writev(sockfd , iv , iv_count);
        
        if(temp < 0){
            if( errno == EAGAIN ){
                modfd(epollfd , sockfd , EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;

        if( bytes_have_send >= iv[0].iov_len){
            iv[0].iov_len = 0;
            iv[1].iov_base = file_address + (bytes_have_send - write_idx);
            iv[1].iov_len = bytes_to_send;
        }else {
            iv[0].iov_base = write_buf + bytes_have_send;
            iv[0].iov_len = iv[0].iov_len - bytes_have_send;
        }

        if(bytes_to_send <= 0){
            unmap();
            modfd(epollfd , sockfd , EPOLLIN);

            if(linger){
                init();
                return true;
            }else {
                return false;
            }
        }
    }
}
/*
    根据响应报文的格式，生成对应8个部分，均由do_request调用
    bool add_response(const char *formate, ...);
    bool add_content(const char *content);
    bool add_status_line(int status,const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
*/
bool http_connection::add_response(const char *format, ...){
    if(write_idx >= WRITE_BUFFER_SIZE){
        return false;
    }
    va_list arg_list ;
    va_start(arg_list,format);
    int len = vsprintf(write_buf + write_idx, WRITE_BUFFER_SIZE-1,write_idx,format,arg_list);
    if(len >= (WRITE_BUFFER_SIZE-1-write_idx)){
        va_end(arg_list);
        return false;
    }
    write_idx += len;
    va_end(arg_list);

    return true;
}

bool http_connection::add_status_line(int status , const char *title){
    return add_response("%s %d %s\r\n" , "HTTP/1.1" , status , title);
}
bool http_connection::add_headers(int content_len){
    return add_content_length(content_len) && add_linger() && add_blank_line();
}
bool http_connection::add_content_length(int content_len){
    return add_response("Content-Length:%d\r\n",content_len);
}
bool http_connection::add_content_type(){
    return add_response("Content-Type:%s\r\n","text/html");
}
bool http_connection::add_linger(){
    return add_response("Connection:%s\r\n",(linger == true) ? "keep-alive" :"close");
}
bool http_connection::add_blank_line(){
    return add_response("%s","\r\n");
}
bool http_connection::add_content(const char *content){
    return add_response("%s",content);
}
bool http_connection::process_write(HTTP_CODE ret){
    switch (ret)
    {
    case INTERNAL_ERROR:{
        add_status_line(500,error_500_title);
        add_headers(strlen(error_500_form));
        if(!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:{
        add_status_line(404,error_404_title);
        add_headers(strlen(error_404_form));
        if(!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:{
        add_status_line(403,error_403_title);
        add_headers(strlen(error_403_form));
        if(!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:{
        add_status_line(200,ok_200_title);
        if(file_stat.st_size != 0){
            add_headers(file_stat.st_size);
            iv[0].iov_base = write_buf;
            iv[0].iov_len = write_idx;
            iv[1].iov_base = file_address;
            iv[1].iov_len = file_stat.st_size;
            iv_count = 2;
            bytes_to_send = write_idx + file_stat.st_size;
            return true;
        }else {
            const char *ok_string = "<html><body></body><html>";
            add_headers(strlen(ok_string));
            if(!add_content(ok_string)) 
                return false;
        }
    }
    default:
        return false;
    }
    iv[0].iov_base = write_buf;
    iv[0].iov_len = write_idx;
    iv_count = 1;
    bytes_to_send = write_idx;
    return true;
}


void http_connection::process(){
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST){
        modfd(epoll_fd , sockfd , EPOLLIN);
        return ;
    }

    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    modfd(epollfd , sockfd , EPOLLOUT);
}

#endif HTTP_CONNECTION_H