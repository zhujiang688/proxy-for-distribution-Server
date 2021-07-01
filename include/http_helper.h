#ifndef HTTP_HELPER_H
#define HTTP_HELPER_H

#include <string>
#include <cstring>
#include <algorithm>

class HttpHelper {
 public:
  // Request
  static int ParseRequest(const std::string& line);
  // @deprecated by yatesyu, since msg is a json
  static void ParseBody(const std::string& line, std::string& user_name, std::string& passwd);
  static int ParseContentLength(char* header, int line);

  // Response
  static void AddState(std::string& msg, int code);
  static void AddHeader(std::string& msg);
  static void AddContent(std::string& msg, const char* content, int len);

 public:
  static const char* CRLF;
  static const char* code_status[3];
};

const char* HttpHelper::CRLF = "\r\n";
const char* HttpHelper::code_status[3] = {"OK", "Bad Request", "Forbidden"};

int HttpHelper::ParseRequest(const std::string& line) {
  auto pos_blank = line.find(' ');
  if (line.substr(0, pos_blank) != "POST") {
    return 400;  // Bad Request
  }
  if (line[pos_blank + 2] != ' ') {
    return 403;  // Forbidden
  }
  return 200;
}

// only suit for this mini project
int HttpHelper::ParseContentLength(char* header, int len) {
  char* last_end_of_line = header;

  while (last_end_of_line - header < len - 2) {
    char* end_of_line = std::search(last_end_of_line, header + len, CRLF, CRLF + 2);
    std::string line(last_end_of_line, end_of_line);
    auto pos_colon = line.find(':');
    auto key = line.substr(0, pos_colon);
    if (key == "Content-Length" || key == "content-length") {
      return std::stoi(line.substr(pos_colon + 1));
    }

    last_end_of_line = end_of_line + 2;
  }

  return -1;  // impossible: a msg must contain content-length
}

//@deprecate: using json
// we only permit "key1=value1&key2=value2..." for searching
// not support register: suppose user info has been recorded
// condition: only POST can enter this method
//            it has been verified in ParseRequest()
void HttpHelper::ParseBody(const std::string& line, std::string& user_name, std::string& passwd) {
  // username=(.*)&passwd=(.*)
  if (line.substr(0, 8) != "username") {
    return;
  }

  auto pos_and = line.find('&', 8);
  user_name = line.substr(9, pos_and - 9);

  if (line.substr(pos_and + 1, 6) != "passwd") {
    return;
  }
  passwd = line.substr(pos_and + 8);
}

void HttpHelper::AddState(std::string& msg, int code) {
  msg += "HTTP/1.1 " + std::to_string(code) + " " +
         (code == 200 ? code_status[0] : (code == 400 ? code_status[1] : code_status[2])) + CRLF;
}

// This method does not add content length
void HttpHelper::AddHeader(std::string& msg) {
  msg += std::string("Connection: close") + CRLF;
  msg += std::string("Content-type: text/plain; charset=UTF-8") + CRLF;
}

void HttpHelper::AddContent(std::string& msg, const char* content, int len) {
  msg += "Content-Length: " + std::to_string(len) + CRLF + CRLF + content;
}

#endif  // HTTP_HELPER_H
