#pragma once

#include <iostream>
#include <queue>
#include <pthread.h>
#include "Task.hpp"
#include "Log.hpp"

#define NUM 6   //线程数量

class ThreadPool
{
public:
    // 线程池组件(单例模式)
    static ThreadPool *Getinstance()
    {
        static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
        if (signel_instance == nullptr)
        {
            pthread_mutex_lock(&_mutex);
            if (signel_instance == nullptr)
            {
                signel_instance = new ThreadPool();
                signel_instance->InitThreadPool();
            }
            pthread_mutex_unlock(&_mutex);
        }

        return signel_instance;
    }

    // 线程执行的函数
    static void *ThreadRoutine(void *args)
    {
        ThreadPool *tp = (ThreadPool *)args;

        // 不停的从任务队列取任务执行
        while (true)
        {
            Task t;
            tp->PopTask(t); // 从队列中取出任务
            t.ProcessOn();  // 通知任务执行
        }
    }

    // 线程池是否停止运行
    bool IsStop()
    {
        return _stop;
    }

    // 判断任务队列是否为空
    bool TaskQueueIsEmpty()
    {
        return _task_queue.size() == 0 ? true : false;
    }

    // 上锁
    void Lock()
    {
        pthread_mutex_lock(&_lock);
    }

    // 解锁
    void UnLock()
    {
        pthread_mutex_unlock(&_lock);
    }

    // 线程池休眠等待
    void ThreadWait()
    {
        pthread_cond_wait(&_cond, &_lock);
    }

    // 唤醒线程池
    void ThreadWakeup()
    {
        pthread_cond_signal(&_cond);
    }

    // 初始化每个线程
    bool InitThreadPool()
    {
        for (size_t i = 0; i < _num; i++)
        {
            pthread_t tid;
            if (pthread_create(&tid, nullptr, ThreadRoutine, this) != 0)
            {
                LOG(FATAL, "Create Thread Error", nullptr);
                return false;
            }
        }
        LOG(NORMAL, "Create Thread Pool Success!", nullptr);
        return true;
    }

    // 添加任务
    void PushTask(const Task &task)
    {
        Lock();

        _task_queue.push(task);

        UnLock();
        ThreadWakeup(); // 唤醒一个线程执行取任务
    }

    // 取出任务
    void PopTask(Task &task)
    {
        Lock();

        while (TaskQueueIsEmpty())
        {
            // 防止被误唤醒，一旦唤醒就会持有互斥锁
            ThreadWait();
        }
        task = _task_queue.front();
        _task_queue.pop();

        UnLock();
    }

    ~ThreadPool()
    {
        // 释放锁
        pthread_mutex_destroy(&_lock);
        pthread_cond_destroy(&_cond);
    }

private:
    ThreadPool(int num = NUM)
        : _num(num),
          _stop(false)
    {
        // 初始化锁
        pthread_mutex_init(&_lock, nullptr);
        pthread_cond_init(&_cond, nullptr);
    }
    ThreadPool(const ThreadPool &)
    {
    }

private:
    int _num;                     // 最大线程数量
    bool _stop;                   // 线程池运行状态
    std::queue<Task> _task_queue; // 任务队列
    pthread_mutex_t _lock;        // 线程锁
    pthread_cond_t _cond;         // 条件变量

    static ThreadPool *signel_instance; // 单例模式声明
};

ThreadPool *ThreadPool::signel_instance = nullptr; // 单例模式定义