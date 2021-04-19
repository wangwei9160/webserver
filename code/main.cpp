#include <string>

#include "mysql_connection_pool.h"

int main(){

    // 数据库信息，登录名，密码，数据表名
    std::string user = "root";
    std::string passwd = "123456";
    std::string databasename = "logindb";

    // 创建数据库连接池
    mysql_connection_pool *connPool = mysql_connection_pool::GetInstance();
    connPool->init("121.196.160.203",user,passwd,databasename,3306,8);


}