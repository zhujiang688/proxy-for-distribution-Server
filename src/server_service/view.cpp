#include "view.h"
#include "connectionPool.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <assert.h>

inquiryView::inquiryView() {}
inquiryView::~inquiryView() {}

//查询数据库中有没有这一项内容
void inquiryView::process(bufferevent *bev, Json::Value &value)
{
  //LOG_FUNC_TRACE();
  std::string name = value["name"].asString(); //用户名
  std::string pass_word = value["password"].asString();//密码
  Json::Value res;
  res["client_sock"] = value["client_sock"].asInt();
  res["pack_id"] = value["pack_id"].asInt();

  //获取数据库连接
  auto con = ConnectionPool::getInstance()->getConnection();
  //身份验证
  char verify[SQL_LEN];
  sprintf(verify, "select * from worker_information where workername=\"%s\" and password=\"%s\";"
                   ,name.c_str(), pass_word.c_str());
  MYSQL_RES* verify_res = nullptr;
  verify_res = con->query(move(std::string(verify)));
  if(verify_res == nullptr)
  {
    res["message"] = "error";
    Head head;
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  else if(verify_res->row_count == 0)
  {
    res["message"] = "Incorrect user name or password";
    Head head;
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }

  //sql语句
  char sql[SQL_LEN] = {0};
  sprintf(sql, "select * from %s_table where workername=\"%s\";", value["task"].asString().c_str(), name.c_str());
  // std::string sql;
  // sql = "select * from pig_table where workername=\"" + name + "\" and flag=1;";
  LogInfo("%s", sql);
  //连接数据库并查询
  MYSQL_RES *mysqlRes = NULL;
  mysqlRes = con->query(move(std::string(sql)));
  if (mysqlRes == nullptr)
  {
    res["message"] = "error";
    Head head;
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  //std::cout << mysqlRes->row_count << std::endl;
  if (mysqlRes->row_count == 0)
  {
    res["message"] = "You haven't get a ticket";
  }
  else
  {
    char **data = mysql_fetch_row(mysqlRes);
    char a[] = "Your Ticket Number is ";
    res["message"] = strcat(a, data[1]);
  }
  mysql_free_result(mysqlRes);
  //向客户端回复
  Head head;
  head.length = strlen(res.toStyledString().c_str());
  bufferevent_write(bev, (char *)&head, sizeof(Head));
  bufferevent_write(bev, res.toStyledString().c_str(), head.length);
}

//抢票命令
ScrambleView::ScrambleView() {}
ScrambleView::~ScrambleView() {}
void ScrambleView::process(bufferevent *bev, Json::Value &value)
{
  //LOG_FUNC_TRACE();
  std::string name = value["name"].asString();
  std::string pass_word = value["password"].asString();
  std::string task = value["task"].asString();
  std::string start_time = value["start_time"].asString();
  //连接数据库并查询
  auto mysql_con = ConnectionPool::getInstance()->getConnection();
  Json::Value res;
  res["client_sock"] = value["client_sock"].asInt();
  res["pack_id"] = value["pack_id"].asInt();
  Head head;
  std::string sql = "rollback;";

  //身份验证
  char verify[SQL_LEN];
  sprintf(verify, "select * from worker_information where workername=\"%s\" and password=\"%s\";"
                   ,name.c_str(), pass_word.c_str());
  MYSQL_RES* verify_res = nullptr;
  verify_res = mysql_con->query(move(std::string(verify)));
  if(verify_res == nullptr)
  {
    res["message"] = "error";
    Head head;
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  else if(verify_res->row_count == 0)
  {
    res["message"] = "Incorrect user name or password";
    Head head;
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }


  //检查时间
  char current_time[64];
  time_t t = time(0);
  strftime(current_time, sizeof(current_time), "%Y-%m-%d %X", localtime(&t));
  char quiry_time[SQL_LEN];
  sprintf(quiry_time, "select start_time from task where task_name=\"%s\";", task.c_str());
  LogInfo("%s", quiry_time);
  MYSQL_RES *task_start_time = mysql_con->query(move(std::string(quiry_time)));
  if (task_start_time == nullptr)
  {
    res["message"] = "error";
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  else
  {
    std::string task_s_time = mysql_fetch_row(task_start_time)[0];
    std::cout<<task_s_time<<std::endl;
    std::cout<<current_time<<std::endl;
    mysql_free_result(task_start_time);
    if (std::string(current_time) < task_s_time)//没到时间的
    {
      res["message"] = "The start of the scramble for tickets is " + task_s_time;
      head.length = strlen(res.toStyledString().c_str());
      bufferevent_write(bev, (char *)&head, sizeof(Head));
      bufferevent_write(bev, res.toStyledString().c_str(), head.length);
      return;
    }
  }

  //抢票流程
  //开启事务
  std::string sql1 = "begin;";
  LogInfo("%s", sql1.c_str());
  if (!mysql_con->update(move(sql1)))
  {
    res["message"] = "error";
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  //检查是否有票
  char sql2[SQL_LEN] = {0};
  sprintf(sql2, "select * from %s_table where workername=\"%s\";", task.c_str(), name.c_str());
  //std::string sql2 = "select * from pig_table where workername=\"" + name + "\" for update;";
  LogInfo("%s", sql2);
  MYSQL_RES *response2 = NULL;
  response2 = mysql_con->query(move(std::string(sql2)));
  if (response2 == nullptr) //检查失败
  {
    //结束事务
    mysql_free_result(response2);
    mysql_con->update(move(sql));
    res["message"] = "error";
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  else
  {
    if (response2->row_count == 1) //已经抢到过票
    {
      mysql_free_result(response2);
      mysql_con->update(move(sql)); //回滚
      res["message"] = "You have get a ticket, Please don't try again";
      head.length = strlen(res.toStyledString().c_str());
      bufferevent_write(bev, (char *)&head, sizeof(Head));
      bufferevent_write(bev, res.toStyledString().c_str(), head.length);
      return;
    }
    mysql_free_result(response2);
  }

  //之前没有抢到票
  //给库存容量上锁
  char sql4[SQL_LEN];
  sprintf(sql4, "select * from task where task_name=\"%s\" for update;", task.c_str());
  //std::string sql4 = "select * from task where task_name=\"pig\" for update;";
  LogInfo("%s", sql4);
  MYSQL_RES *response4 = NULL;
  response4 = mysql_con->query(move(std::string(sql4)));
  if (response4 == nullptr) //上锁失败
  {
    //结束事务
    mysql_free_result(response4);
    mysql_con->update(move(sql));
    res["message"] = "error";
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  else
  {
    char **data = mysql_fetch_row(response4);
    int total_number = stoi(std::string(data[2]));
    int remain_number = stoi(std::string(data[3]));
    mysql_free_result(response4);
    if (remain_number == 0) //没有库存了
    {
      //结束事务
      mysql_con->update(move(sql));
      res["message"] = "no ticket";
      head.length = strlen(res.toStyledString().c_str());
      bufferevent_write(bev, (char *)&head, sizeof(Head));
      bufferevent_write(bev, res.toStyledString().c_str(), head.length);
      return;
    }

    //还有库存
    //如果还有余票就添加票的记录
    char sql6[SQL_LEN];
    sprintf(sql6, "update task set remain_number=remain_number-1 where task_name=\"%s\";", task.c_str());
    //std::string sql6 = "update task set remain_number=remain_number-1 where task_name=\"pig\";";
    LogInfo("%s", sql6);
    if (mysql_con->update(move(std::string(sql6))))
    {
      char sql7[SQL_LEN];
      sprintf(sql7, "insert into  %s_table values(\"%s\", %d);",
              task.c_str(), name.c_str(), total_number - remain_number + 1);
      //std::string sql7 = "update pig_table set flag=1, ticket_number=" + std::to_string(total_number - remain_number + 1) + " where workername=\"" + name + "\";";
      LogInfo("%s", sql7);
      if (mysql_con->update(move(std::string(sql7))))
      {
        //事务提交
        std::string sql8 = "commit;";
        LogInfo("%s", sql8.c_str());
        if (mysql_con->update(move(sql8)))
        {
          res["message"] = "success";
          head.length = strlen(res.toStyledString().c_str());
          bufferevent_write(bev, (char *)&head, sizeof(Head));
          bufferevent_write(bev, res.toStyledString().c_str(), head.length);
        }
        else
        {
          mysql_con->update(move(sql));
          res["message"] = "error";
          head.length = strlen(res.toStyledString().c_str());
          bufferevent_write(bev, (char *)&head, sizeof(Head));
          bufferevent_write(bev, res.toStyledString().c_str(), head.length);
          return;
        }
      }
      else
      {
        mysql_con->update(move(sql));
        res["message"] = "error";
        head.length = strlen(res.toStyledString().c_str());
        bufferevent_write(bev, (char *)&head, sizeof(Head));
        bufferevent_write(bev, res.toStyledString().c_str(), head.length);
        return;
      }
    }
    else //结束事务
    {
      mysql_con->update(move(sql));
      res["message"] = "error";
      head.length = strlen(res.toStyledString().c_str());
      bufferevent_write(bev, (char *)&head, sizeof(Head));
      bufferevent_write(bev, res.toStyledString().c_str(), head.length);
      return;
    }
  }
}

//添加新的抢票任务
CreatetaskView::CreatetaskView() {}
CreatetaskView::~CreatetaskView() {}
void CreatetaskView::process(bufferevent *bev, Json::Value &value)
{
  auto mysql_con = ConnectionPool::getInstance()->getConnection();
  std::string name = value["name"].asString();
  std::string pass_word = value["password"].asString();
  Json::Value res;
  Head head;
  res["client_sock"] = value["client_sock"].asInt();
  res["pack_id"] = value["pack_id"].asInt();

  //身份验证
  char verify[SQL_LEN];
  sprintf(verify, "select * from worker_information where workername=\"%s\" and password=\"%s\";"
                   ,name.c_str(), pass_word.c_str());
  MYSQL_RES* verify_res = nullptr;
  verify_res = mysql_con->query(move(std::string(verify)));
  if(verify_res == nullptr)
  {
    res["message"] = "error";
    Head head;
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  else if(verify_res->row_count == 0)
  {
    res["message"] = "Incorrect user name or password";
    Head head;
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }


  //检查权限
  std::string sql1 = "select count(*) from worker_information where workername=\"" + name + "\" and level>=3;";
  MYSQL_RES *response1 = NULL;
  response1 = mysql_con->query(move(sql1));
  if (response1 == nullptr) //查询失败
  {
    mysql_free_result(response1);
    res["message"] = "error";
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  int count = stoi(std::string(mysql_fetch_row(response1)[0]));
  mysql_free_result(response1);
  if (count == 0) //等级不够
  {
    res["message"] = "Your degree is too low to operate";
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  //开启事务
  std::string roll_back = "rollback;";

  std::string sql2 = "begin;";
  if (!mysql_con->update(move(sql2)))
  {
    res["message"] = "error";
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  //新建任务
  char sql3[SQL_LEN] = {0};
  sprintf(sql3, "insert into task (task_name, start_time, Totoal_number, remain_number) values (\"%s\", \"%s\", %d, %d);",
          value["task_name"].asString().c_str(), value["start_time"].asString().c_str(), value["Total_number"].asInt(), value["Total_number"].asInt());
  LogInfo("%s", sql3);
  if (!mysql_con->update(move(std::string(sql3))))
  {
    while (!mysql_con->update(move(roll_back)))
    {
    };
    res["message"] = "error";
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  //新建任务表
  char sql4[SQL_LEN] = {0};
  sprintf(sql4, "create table %s_table (workername varchar(50), ticket_number int default NULL, primary key(workername));", value["task_name"].asCString());
  LogInfo("%s", sql4);
  if (!mysql_con->update(move(std::string(sql4))))
  {
    while (!mysql_con->update(move(roll_back)))
    {
    };
    res["message"] = "error";
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  // //添加数据
  // char sql5[SQL_LEN] = {0};
  // sprintf(sql5, "insert into %s_table (workername) select workername from worker_information;", value["task_name"].asString().c_str());
  // std::cout << sql5 << std::endl;
  // if (!mysql_con->update(move(std::string(sql5))))
  // {
  //   while (!mysql_con->update(move(roll_back)))
  //   {
  //   };
  //   res["message"] = "error";
  //   head.length = strlen(res.toStyledString().c_str());
  //   bufferevent_write(bev, (char *)&head, sizeof(Head));
  //   bufferevent_write(bev, res.toStyledString().c_str(), head.length);
  //   return;
  // }
  //提交事务
  std::string sql6 = "commit;";
  if (!mysql_con->update(move(sql6)))
  {
    while (!mysql_con->update(move(roll_back)))
    {
    };
    res["message"] = "error";
    head.length = strlen(res.toStyledString().c_str());
    bufferevent_write(bev, (char *)&head, sizeof(Head));
    bufferevent_write(bev, res.toStyledString().c_str(), head.length);
    return;
  }
  res["message"] = "success";
  head.length = strlen(res.toStyledString().c_str());
  bufferevent_write(bev, (char *)&head, sizeof(Head));
  bufferevent_write(bev, res.toStyledString().c_str(), head.length);
}