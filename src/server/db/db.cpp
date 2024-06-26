#include "db.h"
#include <muduo/base/Logging.h>
#include <string> 
#include <iostream>
static std::string server = "127.0.0.1" ; 
static std::string user = "root" ;
static std::string password = "123456" ; 
static std::string dbname = "chat" ; 



MySQL::MySQL(){
    _conn = mysql_init(nullptr) ; 
}

MySQL::~MySQL() {
    if(_conn != nullptr ) {
        mysql_close(_conn) ; 
    }
}

bool MySQL::connect() {
    MYSQL* p = mysql_real_connect(_conn  , server.c_str() , user.c_str() 
    , password.c_str() , dbname.c_str() , 3306 , nullptr , 0 ) ;
    if(p != nullptr ) {
        // mysql_query(_conn , "set names gbk") ; 
        LOG_INFO << "connection mysql database sucess!" ; 
    } else {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << "连接失败，请稍后重试" ; 
        std::cerr << mysql_error(_conn) << std::endl ; 
    }
    return p ; 
}

bool MySQL::update(std::string sql) {
    if(mysql_query(_conn , sql.c_str() ) ) {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败" ; 
        return false ; 
    }
    return true ; 
}

MYSQL_RES* MySQL::query(std::string sql) {
    if(mysql_query(_conn , sql.c_str())) {
            LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "查询失败" ; 
        return nullptr ; 
    }
    return mysql_use_result(_conn) ; 
}

MYSQL* MySQL::getConnection(){
    return _conn ; 
}