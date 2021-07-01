#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include <map>
#include <json/json.h>  //json库
#include "view.h"       //不同的消息处理对象

class Controller {
   private:
    std::map<int, View*> m_modelMap;  // int是消息的类型， View*处理该类型消息的对象
   public:
    Controller();
    ~Controller();
    void process(bufferevent* bev, Json::Value& value);  // fd是客户端的套接字， value是客户端发来的消息
};

#endif
