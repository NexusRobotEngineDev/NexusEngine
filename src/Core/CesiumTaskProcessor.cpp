#include "CesiumTaskProcessor.h"
#include <algorithm>

CesiumTaskProcessor::CesiumTaskProcessor(size_t numThreads) : m_stop(false) {
    size_t actualThreads = std::max<size_t>(1, numThreads);
    for (size_t i = 0; i < actualThreads; ++i) {
        m_workers.emplace_back([this] { workerThread(); });
    }
}

CesiumTaskProcessor::~CesiumTaskProcessor() {
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }
    m_condition.notify_all();

    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void CesiumTaskProcessor::startTask(std::function<void()> f) {
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_tasks.push(std::move(f));
    }
    m_condition.notify_one();
}

void CesiumTaskProcessor::workerThread() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_condition.wait(lock, [this] { return m_stop || !m_tasks.empty(); });

            if (m_stop && m_tasks.empty()) {
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        if (task) {
            task();
        }
    }
}
