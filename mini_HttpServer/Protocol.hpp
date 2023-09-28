#pragma once

#include <iostream>
#include <cstdlib>
#include <sstream>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include "Util.hpp"
#include "Log.hpp"

#define SEP ": "                // 每行请求正文中间的分隔符
#define WEB_ROOT "wwwroot"      // Wab根目录
#define HOME_PAGE "index.html"  // 网站首页
#define HTTP_VERSION "HTTP/1.1" // 响应的版本
#define LINE_END "\r\n"         // 响应的行分隔符
#define PAGE_404 "404.html"     // 404错误访问的主页

// 状态码
enum ERR_CODE
{
    OK = 200,          // 正常
    BAD_REQUEST = 400, // 请求方法不正确
    NOT_FOUND = 404,   // 请求访问文件不存在
    SERVER_ERROR = 500 // 服务器内部执行错误
};

// 描述状态码
static std::string Code2Desc(int code)
{
    std::string desc;
    switch (code)
    {
    case 200:
        desc = "OK";
        break;
    case 404:
        desc = "Not Found";
        break;
    default:
        break;
    }

    return desc;
}

static std::string Suffix2Desc(const std::string &suffix)
{
    static std::unordered_map<std::string, std::string> suffix2desc = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".jpg", "application/xml"},
        {".xml", "application/xml"}};

    auto iter = suffix2desc.find(suffix);
    if (iter != suffix2desc.end())
        return iter->second;

    return "text/html";
}

// Http请求结构
class HttpRequest
{
public:
    // 暂存缓冲区
    std::string _request_line;                // 请求状态行
    std::vector<std::string> _request_header; // 存储Http请求报头的数组
    std::string _blank;                       // 空行
    std::string _request_body;                // 请求的主体正文

    // 解析完毕之后的状态行结果
    std::string _method;  // [方法]
    std::string _uri;     // [URL]
    std::string _version; // [版本]

    std::unordered_map<std::string, std::string> _header_kv; // 拆分请求报头的属性名和属性值
    int _conent_lenght;                                      // 请求的正文主体长度
    std::string _suffix;                                     // 请求访问文件的后缀类型
    off_t _size;                                             // 请求访问文件的大小

    // 切分URL的缓冲区
    std::string _path;         // 请求访问的路径
    std::string _query_string; // 访问参数

    bool _cgi; // cgi是否使能

    HttpRequest()
        : _conent_lenght(0),
          _size(0),
          _cgi(false)
    {
    }
    ~HttpRequest()
    {
    }
};

// Http响应结构
class HttpResponse
{
public:
    // 暂存缓冲区
    std::string _status_line;                  // 状态首行
    std::vector<std::string> _response_header; // 存储单行Http响应报头的数组
    std::string _blank;                        // 空行
    std::string _response_body;                // 响应的正文

    int _status_code; // 当前Http请求的状态码
    int _fd;          // 打开请求访问的文件的文件描述符

    HttpResponse()
        : _blank(LINE_END),
          _status_code(OK),
          _fd(-1)
    {
    }
    ~HttpResponse()
    {
    }
};

// IO交互通信协议
// 读取请求, 分析请求, 构建响应, 发送响应
class InteractionCenter
{
public:
    InteractionCenter(int sock)
        : _sock(sock), _stop(false)
    {
    }

    // 获取是否停止连接处理的标志位值
    bool IsStop()
    {
        return _stop;
    }

    // 读取请求，分析请求
    void RecvHttpRequest()
    {
        // 读取Http请求状态行、请求报头
        if ((!RecvHttpRequestLine()) && (!RecvHttpRequestHandler()))
        {
            ParseHttpRequestLine();    // 解析Http请求状态行
            ParseHttpRequestHandler(); // 解析Http请求报头
            RecvHttpRequestBody();     // 解析正文主体
        }
    }

    // 构建响应
    void BuildHttpResponse()
    {
        std::string next;
        size_t found = 0;                        // 文件名称后缀的起始下标
        int &code = _http_response._status_code; // 状态码

        if (_http_request._method != "GET" && _http_request._method != "POST")
        {
            // 此时获取到的是非法请求
            LOG(WARNING, "请求的方法不是正确的", nullptr);
            code = BAD_REQUEST;

            goto END;
        }

        if (_http_request._method == "GET")
        {
            // 切分出URL'?'的前后
            // 1. [地址]?[参数]
            // 2. 只有[地址]
            size_t pos = _http_request._uri.find('?');
            if (pos != std::string::npos)
            {
                // 带参数，提取[地址]和[参数]
                Util::CurString(_http_request._uri, _http_request._path, _http_request._query_string, "?");
                _http_request._cgi = true; // 激活cgi
            }
            else
            {
                // 无参数，直接保存访问地址
                _http_request._path = _http_request._uri;
            }
        }

        if (_http_request._method == "POST")
        {
            // "POST"方法参数在请求的主体正文中
            _http_request._cgi = true; // 激活cgi
            _http_request._path = _http_request._uri;
        }

        // 拼接上我们的根目录
        next = _http_request._path;
        _http_request._path = WEB_ROOT;
        _http_request._path += next;

        // 如果请求没有具体访问哪一块内容，则跳转到对应目录的主页
        if (_http_request._path[_http_request._path.size() - 1] == '/')
            _http_request._path += HOME_PAGE;

        // 检测访问资源是否存在
        struct stat st;
        if (stat(_http_request._path.c_str(), &st) == 0)
        {
            // 存在
            // 但请求的资源是一个目录
            if (S_ISDIR(st.st_mode))
            {
                // 每个目录里都要有一个index.html主页
                _http_request._path += "/";
                _http_request._path += HOME_PAGE;
                stat(_http_request._path.c_str(), &st);
            }

            // 请求的是可执行文件，判断这个文件是否能被外部访问并执行
            if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
            {
                // S_IXUSR:文件所有者具可执行权限
                // S_IXGRP:用户组具可执行权限
                // S_IXOTH:其他用户具可执行权限

                _http_request._cgi = true; // 激活cgi
            }
            _http_request._size = st.st_size; // 获取访问文件的大小
        }
        else
        {
            // 访问的资源不存在
            LOG(WARNING, "%s Not Found, sock : %d", _http_request._path.c_str(), _sock);
            code = NOT_FOUND;

            goto END;
        }

        // 找访问的文件名称中后缀的起始位置
        found = _http_request._path.rfind("."); // 反向查找文件中"."所在位置
        if (found = std::string::npos)
            _http_request._suffix = ".html";
        else
            _http_request._suffix = _http_request._path.substr(found);

        // 检测cgi是否激活
        if (_http_request._cgi)
        {
            // cgi激活, 执行目标程序，拿到结果->_http_response._response_body
            code = ProcessCgi();
        }
        else
        {
            // 未激活，普通处理方法
            // 1. 一定是Get方法
            // 2. 没有参数
            // 3. 不是可执行文件
            code = ProcessNonCgi(); // 简单的网页返回，返回静态网页
        }

    END:
        // 处理状态码，不同状态码对应不同处理方法
        BuildHttpResponseHelper();
    }

    // 发送响应
    void SendHttpResponse()
    {
        // 可以打包成一整个字符串发，也可以单独发，都是存到底层缓存区内，但一次发多少是有协议决定的

        // 发送响应状态行
        send(_sock, _http_response._status_line.c_str(), _http_response._status_line.size(), 0);

        // 发送一行行响应报头
        for (auto iter : _http_response._response_header)
            send(_sock, iter.c_str(), iter.size(), 0);

        // 添加正文结尾的空行
        send(_sock, _http_response._blank.c_str(), _http_response._blank.size(), 0);

        if (_http_request._cgi)
        {
            // 发送响应正文中的数据
            std::string &response_body = _http_response._response_body;
            size_t size = 0;
            size_t total = 0;
            const char *start = response_body.c_str();
            while (total < response_body.size() && (size = send(_sock, start + total, response_body.size() - total, 0)) > 0)
            {
                total += size;
            }
        }
        else
        {
            // 发送静态网页文件
            sendfile(_sock, _http_response._fd, nullptr, _http_request._size);
            close(_http_response._fd); // 发送完关闭请求的文件
        }
    }

    ~InteractionCenter()
    {
        close(_sock);
    }

private:
    // 读取Http请求状态行
    bool RecvHttpRequestLine()
    {
        std::string &line = _http_request._request_line;
        if (int ret = Util::ReadLine(_sock, line) > 0)
        {
            line.resize(line.size() - 1); // 去除单行末尾的"\n"
            LOG(NORMAL,"%s, sock: %d",line.c_str(), _sock);
        }
        else
        {
            _stop = true;
            LOG(WARNING, "User Exit Or ReadLine() Error, ret = %d sock : %d",ret, _sock);
        }

        return _stop;
    }

    // 读取出Http请求报头
    bool RecvHttpRequestHandler()
    {
        std::string line;
        while (true)
        {
            line.clear();

            // 提取一行请求报头放入line中
            if (Util::ReadLine(_sock, line) <= 0)
            {
                _stop = true;
                LOG(NORMAL, "User Exit Or ReadLine() Error, sock : %d", _sock);
                break;
            }
            if (line == "\n")
            {
                // 请求报头最后一行为空行
                _http_request._blank = line;
                //LOG(NORMAL, "读取请求报头结束", nullptr);
                break;
            }

            line.resize(line.size() - 1);                  // 去除每行末尾的"\n"
            _http_request._request_header.push_back(line); // 将单行请求报头存入数组
            //LOG(NORMAL, line, nullptr);
        }

        return _stop;
    }

    // 解析状态行
    void ParseHttpRequestLine()
    {
        std::string &line = _http_request._request_line;

        // stringstream流会自动以空格分割字符串
        std::stringstream ss(line);
        ss >> _http_request._method >> _http_request._uri >> _http_request._version;

        // 全部转换为大写字母，统一规范
        Util::ToUpperCase(_http_request._method);
        // Util::ToUpperCase(_http_request._uri);
        Util::ToUpperCase(_http_request._version);
    }

    // 解析出每一行请求报头
    void ParseHttpRequestHandler()
    {
        std::string key;   // 属性名的缓冲区
        std::string value; // 属性值的缓冲区
        for (auto &iter : _http_request._request_header)
        {
            // 拆分属性名和属性值，用map对应存储
            if (Util::CurString(iter, key, value, SEP))
                _http_request._header_kv.insert(std::make_pair(key, value));
        }
    }

    // 获取Http请求中关于主体正文大小的解释
    bool IsNeedRecvHttpRequestBody()
    {
        std::string method = _http_request._method; // 首行的方法
        if (method == "POST")
        {
            auto &head_kv = _http_request._header_kv;
            auto iter = head_kv.find("Content-Length");
            if (iter != head_kv.end())
            {
                LOG(NORMAL, "Method = Post, Conent-Lenght : %s, sock: %d", iter->second.c_str(), _sock);
                _http_request._conent_lenght = atoi(iter->second.c_str());
                return true;
            }
        }
        LOG(NORMAL, "NO Request Body, sock: %d", _sock);

        return false;
    }

    // 将正文主体提取到正文缓冲区
    bool RecvHttpRequestBody()
    {
        // 没有识别到主体正文大小直接退出
        if (!IsNeedRecvHttpRequestBody())
            return false;

        char ch = 0;                                      // 单字符缓冲区
        int conent_lenght = _http_request._conent_lenght; // 主体正文的长度
        std::string &body = _http_request._request_body;  // 主体正文缓冲区

        // 循环读取主体正文，保存到主体正文缓冲区
        while (conent_lenght)
        {
            ssize_t n = recv(_sock, &ch, 1, 0);
            if (n > 0)
            {
                body.push_back(ch);
                conent_lenght--;
            }
            else
            {
                _stop = true;
                LOG(NORMAL, "User Exit Or recv() Error, sock : %d", _sock);
                break;
            }
        }
        LOG(NORMAL, "body : %s", body.c_str());

        return _stop;
    }

    int ProcessCgi()
    {
        LOG(NORMAL, "Use CGI Model", nullptr);

        int code = OK;
        std::string &method = _http_request._method;                // 请求的方法
        std::string &bin = _http_request._path;                     // 子进程执行的目标程序
        int content_lenght = _http_request._conent_lenght;          // 请求主体正文大小
        std::string &response_body = _http_response._response_body; // 响应正文

        std::string &query_string = _http_request._query_string; // GET方法的参数
        std::string &body_text = _http_request._request_body;    // POST方法的参数

        // 创建站在父进程角度的两个无名管道
        int input[2];  // 父进程读取
        int output[2]; // 父进程发送

        if (pipe(input) < 0)
        {
            LOG(ERROR, "pipe input error", nullptr);
            code = SERVER_ERROR;
            return code;
        }

        if (pipe(output) < 0)
        {
            LOG(ERROR, "pipe output error", nullptr);
            code = SERVER_ERROR;
            return code;
        }

        pid_t pid = fork();
        if (0 == pid)
        {
            // 子进程
            close(input[0]);  // 子进程写管道，关闭父进程读
            close(output[1]); // 子进程读管道，关闭父进程写

            // 将请求的方法添加至子进程的环境变量中
            setenv("METHOD", method.c_str(), 1);

            if (method == "GET")
            {
                // 如果是GET方法，将GET方法的参数添加至子进程的环境变量中
                setenv("QUERY_STRING", query_string.c_str(), 1);
                LOG(NORMAL, "Get Method, Add Query_String Env", nullptr);
            }
            else if (method == "POST")
            {
                // 如果是POST方法, 将请求主体正文的大小添加至子进程的环境变量中
                setenv("CONTENT_LENGHT", std::to_string(content_lenght).c_str(), 1);
                LOG(NORMAL, "Post Method, Add Content-Lenght Env, Env = %d, sock : %d", content_lenght, _sock);
            }
            else
            {
                // Do Nothing
            }

            // 进程替换，执行成功，只替换代码和数据，并不替换内核进程相关的数据结构
            // 比如：文件描述符表，子进程中之前打开的文件描述符依然在表中，替换后只是不知道具体编号
            dup2(input[1], 1);  // 子进程写口，重定向到标准输出
            dup2(output[0], 0); // 子进程读口，重定向到标准输入
            // 这样即使进程替换之后，不知道俩管道的编号，但它们已经与标准输入、标准输出绑定

            execl(bin.c_str(), bin.c_str(), nullptr);
            exit(1);
        }
        else if (pid < 0)
        {
            LOG(ERROR, "fork error", nullptr);
            code = 404;
            return code;
        }
        else
        {
            // 父进程
            close(input[1]);  // 父进程读管道，关闭子进程写
            close(output[0]); // 父进程写管道，关闭子进程读

            // 将POST方法的参数写给子进程
            if (method == "POST")
            {
                const char *start = body_text.c_str();
                size_t total = 0;
                size_t size = 0;
                while (total < content_lenght && (size = write(output[1], start + total, body_text.size() - total)) > 0)
                {
                    total += size;
                }
            }

            // 接收CGI处理完的数据，然后存储到响应正文中
            char ch = 0;
            while (read(input[0], &ch, 1) > 0)
                response_body.push_back(ch);

            int status = 0;
            pid_t ret = waitpid(pid, nullptr, 0); // 父进程阻塞，等待子进程退出
            if (ret == pid)
            {
                // WIFEXITED如果子进程正常结束，它就返回真；否则返回假
                if (WIFEXITED(status))
                {
                    // WEXITSTATUS该宏取得子进程exit()返回的结束代码
                    if (WEXITSTATUS(status) == 0)
                        code = OK;
                    else
                        code = BAD_REQUEST;
                }
                else
                {
                    code = SERVER_ERROR;
                }
            }
            else
            {
                LOG(ERROR, "waitpid error", nullptr);
                code = SERVER_ERROR;
            }

            // 关闭父进程的读写俩管道
            close(input[0]);
            close(output[1]);
        }

        return code;
    }

    // 返回静态网页
    int ProcessNonCgi()
    {
        // 打开请求的文件
        _http_response._fd = open(_http_request._path.c_str(), O_RDONLY);
        if (_http_response._fd < 0)
            return 404;

        return OK;
    }

    // 状态码正确的响应报头构建
    void BuildOkResponse()
    {
        // 设置响应报头中实体正文的类型
        std::string line = "Content-Type: ";
        line += Suffix2Desc(_http_request._suffix);
        line += LINE_END;
        _http_response._response_header.push_back(line);

        // 设置响应报头中实体正文的大小
        line = "Content-Lenght: ";
        if (_http_request._cgi)
            line += std::to_string(_http_response._response_body.size()); // POST
        else
            line += std::to_string(_http_request._size); // GET

        line += LINE_END;
        _http_response._response_header.push_back(line);
    }

    // 状态码错误给用户返回的404页面
    void HandlerError(std::string page)
    {
        _http_response._fd = open(page.c_str(), O_RDONLY);
        if (_http_response._fd > 0)
        {
            struct stat st;
            stat(PAGE_404, &st);

            // 设置响应报头中实体正文的类型
            std::string line = "Content-Type: text/html";
            line += LINE_END;
            _http_response._response_header.push_back(line);

            // 设置响应报头中实体正文的大小
            line = "Content-Lenght: ";
            line += std::to_string(st.st_size);
            line += LINE_END;
            _http_response._response_header.push_back(line);

            // 请求文件的大小变成404页面文件的大小
            _http_request._size = st.st_size;
        }
    }

    // 处理状态码
    void BuildHttpResponseHelper()
    {
        int &code = _http_response._status_code; // 获取状态码

        // 构建响应状态行: [版本] [状态码] [状态码描述] [行分隔符]
        std::string &status_line = _http_response._status_line;
        status_line += HTTP_VERSION;
        status_line += " ";
        status_line += std::to_string(code);
        status_line += " ";
        status_line += Code2Desc(code);
        status_line += _http_response._blank;

        // 构建响应正文，可能包括响应报头
        std::string path = WEB_ROOT;
        path += "/";
        switch (code)
        {
        case OK:
            BuildOkResponse(); // 构建响应报头
            break;

        case NOT_FOUND:
            LOG(WARNING, "处理错误状态码: %d, sock : %d", code, _sock);
            path += PAGE_404;
            HandlerError(path); // 状态码错误给用户返回的404页面
            break;

        case BAD_REQUEST:
            break;

        case SERVER_ERROR:
            break;

        default:
            break;
        }
    }

private:
    int _sock;
    HttpRequest _http_request;   // 接收处理缓冲区
    HttpResponse _http_response; // 构建发送缓冲区
    bool _stop;                  // 标志位，是否停止处理用户连接
};

class CallBack
{
public:
    CallBack()
    {
    }

    // 仿函数入口，执行任务
    void operator()(int sock)
    {
        HandlerRequest(sock);
    }

    void HandlerRequest(int sock)
    {
        LOG(NORMAL, "Handler Hequest Begin, sock: %d", sock);

#ifdef DEBUG
        char buffer[4096];
        recv(sock, buffer, sizeof(buffer), 0);

        std::cout << "----------------begin----------------" << std::endl;
        std::cout << buffer << std::endl;
        std::cout << "-----------------end-----------------" << std::endl;
#else
        // 开始通讯
        InteractionCenter *ic = new InteractionCenter(sock);

        // 1. 读取并分析Http请求
        ic->RecvHttpRequest();
        if (ic->IsStop())
        {
            LOG(WARNING, "Recv Error, Stop Build And Send, sork : %d", sock);
        }
        else
        {
            // 2. 根据分析的请求构建响应
            ic->BuildHttpResponse();
            // 3. 发送构建的响应
            ic->SendHttpResponse();
        }
        // 通讯结束
        delete ic;
#endif
    }

    ~CallBack()
    {
    }
};