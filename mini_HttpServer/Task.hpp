#pragma once

#include <iostream>
#include "Protocol.hpp"

class Task
{
public:
    Task()
    {
    }
    Task(int sock)
        : _sock(sock)
    {
    }

    // 通知执行任务
    void ProcessOn()
    {
        _handler(_sock); // 调用执行对象的仿函数
    }

    ~Task()
    {
    }

private:
    int _sock;
    CallBack _handler; // 业务执行对象
};