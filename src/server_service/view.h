#ifndef _VIEW_H_
#define _VIEW_H_
#include <json/json.h>
#include <event2/bufferevent.h>
#include <time.h>
#include "structure.h"
#include "mini_log.h"

class View {
   public:
    virtual void process(bufferevent* bev, Json::Value& value) = 0;
};

//查询有没有抢到票
class inquiryView : public View {
   public:
    inquiryView();
    ~inquiryView();

    void process(bufferevent* bev, Json::Value& value);
};

//抢票
class ScrambleView : public View {
   public:
    ScrambleView();
    ~ScrambleView();
    void process(bufferevent* bev, Json::Value& value);
};

class CreatetaskView : public View {
   public:
    CreatetaskView();
    ~CreatetaskView();
    void process(bufferevent* bev, Json::Value& value);
};

#endif