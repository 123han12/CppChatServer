#include "chatservice.hpp"
#include "public.hpp"
#include <vector>
#include <string>

ChatService* ChatService::instance() {
    static ChatService service ; 
    return &service ; 
} 

/*
    为了实现数据库和业务层的分离，这里需要添加一个中间类，从而方便后期的数据库的更换的时候，
    对于业务层的代码不会有一个较大的改动，从而实现了解耦
*/


// 这里需要做一些初始化操作
ChatService::ChatService() {

    // 注册好事件对应的处理函数
    _msgHandlerMap.insert({LOGIN_MSG , bind(&ChatService::login , this , _1 , _2 , _3 ) } ) ; 
    _msgHandlerMap.insert({REG_MSG , bind(&ChatService::Register , this , _1 , _2 , _3 ) } ) ; 
    _msgHandlerMap.insert({ONE_CHAT_MSG , bind(&ChatService::OneChat , this , _1 , _2 , _3 ) } ) ; 

}


// 处理登录业务
void ChatService::login(const TcpConnectionPtr& conn , json& js , Timestamp time ){
    int id = js["id"] ; 
    std::string pwd = js["password"] ; 
    User user = _userModel.query(id) ; 
    if(user.getId() != -1 && user.getPwd() == pwd ) { // 存在用户信息
        if(user.getState() == std::string( "online") ) {   // 如果此时状态已经是 online 则错误
            json response ; 
            response["msgid"] = LOGIN_MSG_ACk ; 
            response["error"] = -1 ; 
            response["errmsg"] = "user aleary online , please attempt next time" ; 
            conn->send(response.dump()) ; 
        }else {
            
            // 
            {
                lock_guard<mutex> lock(_connMutex) ;  // 这种对象构造的时候加锁，析构的时候解锁 
                _connMap.insert(make_pair(id , conn) ) ;  //在线程互斥的情况下将其进行插入 
            }

            user.setState(std::string("online")) ; 
            _userModel.updateState(user) ;  // 更新这个用户的状态 
            json response ; 
            response["msgid"] = LOGIN_MSG_ACk ; 
            response["error"] = 0 ; 
            response["errmsg"] = "login sucess!" ; 

            // 从离线表中取出这个对象的所有离线的消息
            std::vector<std::string> vec = _offlineMessageModel.query(id) ; 
            if(!vec.empty() ) {
                response["offlinemsg"] = vec ; 
                _offlineMessageModel.remove(id) ;  // 移除已经被取出的信息
            }

            conn->send(response.dump()) ; 
        }
    } else {  // 不存在用户信息
        json response ; 
        response["msgid"] = LOGIN_MSG_ACk ; 
        response["error"] = -1 ; 
        response["errmsg"] = "user not exists" ; 
        conn->send(response.dump()) ;
    }
} 


// 处理注册业务
void ChatService::Register(const TcpConnectionPtr& conn , json& js , Timestamp time ) {
    // 实现注册业务
    User user ; 

    std::string name = js["name"] ; 
    std::string password = js["password"] ; 

    user.setName(name) ; 
    user.setPwd(password) ; 

    UserModel model ; 
    bool state  = _userModel.insert(user) ;  
    
    if(state) {    // 注册成功  
        json response ; 
        response["msgid"] = REG_MSG_ACK ; 
        response["error"] = 0 ; 
        response["id"] = user.getId() ; 
        conn->send(response.dump()) ; 
    }else {         // 注册失败
        json response ; 
        response["msgid"] = REG_MSG_ACK ; 
        response["error"] = -1 ; 
        response["errmsg"] = "register error" ; 
        conn->send(response.dump()) ;
    }   

}

MsgHandler ChatService::getMsgHandler(int msgid) {
    if(_msgHandlerMap.find(msgid) == _msgHandlerMap.end() ) {
        return [](const TcpConnectionPtr& , json&  , Timestamp ) -> void {
            LOG_ERROR << "the msgid not have a handler function" ; 
        } ; 
    } 
    return _msgHandlerMap[msgid] ; 
}


void ChatService::clientCloseException(const TcpConnectionPtr& conn ) {
    
    User user ; 
    {
        lock_guard<mutex> lock(_connMutex) ; 
        for(auto iter = _connMap.begin() ; iter != _connMap.end() ; iter ++ ) {
            if(iter->second == conn ){
                user.setId(iter->first) ; 
                _connMap.erase(iter) ; 
                break ;  
            }
        }
    } 

    // 更新数据库状态
    user.setState(std::string("offline"));
    _userModel.updateState(user) ; 

}


void ChatService::OneChat(const TcpConnectionPtr& conn , json& js , Timestamp time ){

    int toid = js["to"].get<int>(); 
    {
        lock_guard<mutex> lock(_connMutex) ; 
        auto iter = _connMap.find(toid) ; 
        if(iter != _connMap.end() ) {  // 表示此时对端在线，我们就可以进行发送数据了 
            iter->second->send(js.dump() ) ;  // 直接转发
            return  ; 
        }
    }

    // 如果对端处于离线状态，则先将信息存储到离线表中(此时先不考虑集群的情景)
    _offlineMessageModel.insert(toid , js.dump() ) ; 
}


// 将所有人的状态修改为下线
void ChatService::reset() {
    _userModel.resetState() ;
     
}

