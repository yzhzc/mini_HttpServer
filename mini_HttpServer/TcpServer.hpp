#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <unistd.h>
#include <memory>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>
#include <fcntl.h>
#include "Log.hpp"

#define BACKLOG 100 // listen的第二个参数,底层全连接队列的长度 = listen的第二个参数+1

// 一般经验
// const std::string &: 输入型参数
// std::string *: 输出型参数
// std::string &: 输入输出型参数.

class TcpServer
{
public:
    static TcpServer *GetInstance(int port)
    {
        // 单例模式(需要时创建模式)
        // 该模式创建时保证多线程安全
        static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        if (nullptr == svr)
        {
            pthread_mutex_lock(&lock);
            if (nullptr == svr)
            {
                svr = new TcpServer(port);
                svr->InitServer(); // 初始化套接字
            }
            pthread_mutex_unlock(&lock);
        }
        return svr;
    }

    // 创建套接字绑定监听
    void InitServer()
    {
        Socket();
        Bind();
        Listen();
    }

    void Socket()
    {
        _listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (_listen_sock < 0)
        {
            LOG(FATAL, "create socket error, 错误代码: %d: %s", errno, strerror(errno));
            exit(1);
        }
        int opt = 1;

        // 设置套接字属性setsockopt(套接字, 协议层, 选项, 返回值缓冲区, 缓冲区大小)
        // SO_REUSEADDR 端口复用, TCP中一方断开另一方会有一个timewait等待时间
        // SO_REUSEPORT 使多进程或者多线程创建多个绑定同一个ip:port的监听socket
        setsockopt(_listen_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
        LOG(NORMAL, "创建socket成功, _listen_sock: %d", _listen_sock);
    }

    void Bind()
    {
        struct sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(_port);      // 将端口号转为大端数据
        local.sin_addr.s_addr = INADDR_ANY; // 云服务器不能直接绑定公网IP

        if (bind(_listen_sock, (struct sockaddr *)&local, sizeof(local)) < 0)
        {
            LOG(FATAL, "bind error, 错误代码: %d : %s", errno, strerror(errno));
            exit(2);
        }
    }

    void Listen()
    {
        if (listen(_listen_sock, BACKLOG) < 0)
        {
            LOG(FATAL, "listen error, 错误代码: %d:%s", errno, strerror(errno));
            exit(3);
        }
        LOG(NORMAL, "监听中, sock: %d", _listen_sock);
    }

    // 获取当前套接字
    int Sock()
    {
        return _listen_sock;
    }

    // 设置文件描述符为非阻塞
    static bool SetNonBlock(int sock)
    {
        int fl = fcntl(sock, F_GETFL);
        if (fl < 0)
            return false;

        fcntl(sock, F_SETFL, fl | O_NONBLOCK);

        return true;
    }

    ~TcpServer()
    {
        if (_listen_sock >= 0)
            close(_listen_sock);
    }

private:
    TcpServer(uint16_t port)
        : _port(port),
          _listen_sock(-1)
    {
    }

    TcpServer(const TcpServer &) {}

private:
    uint16_t _port;
    int _listen_sock;
    static TcpServer *svr; // 单例模式声明
};

TcpServer *TcpServer::svr = nullptr; // 单例模式定义