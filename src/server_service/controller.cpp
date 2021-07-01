#include "controller.h"


Controller::Controller() {
  //消息处理的准备工作，为不同的消息类型准备不同的View对象
  m_modelMap.insert(std::make_pair(EN_INQUIRY, new inquiryView()));
  m_modelMap.insert(std::make_pair(EN_SCRAMBLE, new ScrambleView()));
  m_modelMap.insert(std::make_pair(EN_CREATE, new CreatetaskView()));
  LogInfo("创建view对象成功");
}

Controller::~Controller() {
  for (auto x=m_modelMap.begin(); x!=m_modelMap.end(); ++x) {
    delete x->second;
  }
}

void Controller::process(bufferevent* bev, Json::Value& value) {
  LogInfo("处理用户请求");
  m_modelMap[value["type"].asInt()]->process(bev, value);//根据消息的类型，选择不同的处理函数进行处理
}