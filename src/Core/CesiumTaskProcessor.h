#pragma once

#include "thirdparty.h"
#include <CesiumAsync/ITaskProcessor.h>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

/**
 * @brief CesiumNative 默认的任务处理器，维护一个后台线程池来分发并发任务。
 */
class CesiumTaskProcessor : public CesiumAsync::ITaskProcessor {
public:
    /**
     * @brief 构造函数，初始指定后台工作线程的数量
     * @param numThreads 工作线程数量，默认为系统最大并发核心数
     */
    explicit CesiumTaskProcessor(size_t numThreads = std::thread::hardware_concurrency());

    /**
     * @brief 析构函数，安全关闭和清理后台任务线程
     */
    ~CesiumTaskProcessor() override;

    /**
     * @brief 向线程池派发一个新的异步执行任务
     * @param f 准备在后台执行的函数对象
     */
    void startTask(std::function<void()> f) override;

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;

    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    bool m_stop;

    void workerThread();
};
