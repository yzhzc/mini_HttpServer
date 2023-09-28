#pragma once

#include <iostream>
#include <pthread.h>
#include <signal.h>
#include "TcpServer.hpp"
#include "Protocol.hpp"
#include "Task.hpp"
#include "ThreadPool.hpp"

#define PORT 8888

class HttpServer
{
public:
    HttpServer(int port = PORT)
        : _port(port),
          _state(true)
    {
    }

    // 启动tcp服务
    void InitServer()
    {
        // 忽略SIGPIPE信号，否则在写入时，server可能随时崩溃
        // SIGPIPE: 对已经关闭的对象write产生
        signal(SIGPIPE, SIG_IGN);
    }

    void Loop()
    {
        LOG(NORMAL, "正在循环接收", nullptr);

        // 创建TCP组件
        TcpServer *tsvr = TcpServer::GetInstance(_port);

        while (_state)
        {
            struct sockaddr_in peer;
             socklen_t len = sizeof(peer);
            int sock = accept(tsvr->Sock(), (struct sockaddr *)&peer, &len);
            if (sock < 0)
            {
                if ((errno == ECONNABORTED) || (errno == EINTR)) // 如果是被信号中断和软件层次中断,不能退出
                {
                    continue;
                }
                else
                {
                    // 非阻塞时,不停read,但是没有可读数据时，系统自动优化停止read,返回EAGAIN, window平台叫EWOULDBLOCK
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        LOG(WARNING, "accept error, %d : %s", errno, strerror(errno));
                        continue;
                    }
                    LOG(ERROR, "accept error, %d : %s", errno, strerror(errno));

                    return;
                }
            }
            LOG(NORMAL, "accept提取到新连接, newSock: %d", sock);

            // 布置任务对象
            Task task(sock);

            // 创建线程池组件
            // 将任务添加至线程池执行队列
            ThreadPool::Getinstance()->PushTask(task);
        }
    }

    ~HttpServer() {}

private:
    int _port;   // 端口号
    bool _state; // 判断http服务是否在运行
};