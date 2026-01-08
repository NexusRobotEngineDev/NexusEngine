#pragma once

#include "GlobalSceneTable.h"
#include "DrawCommandGenerator.h"
#include "CommonTypes.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <functional>
#include <atomic>

namespace Nexus {
namespace Core {

/**
 * @brief 流式加载管理器，负责异步 trunk 的加载/卸载并同步 MDI 指令缓冲
 *
 * 后台线程处理 IO 任务，主线程调用 flush() 将已完成任务的结果
 * 更新到 DrawCommandGenerator，保证渲染线程不被阻塞。
 */
class StreamingManager {
public:
    /**
     * @brief 构造流式管理器
     * @param sceneTable 全局场景表引用
     * @param commandGen MDI 指令生成器引用
     */
    StreamingManager(GlobalSceneTable* sceneTable, DrawCommandGenerator* commandGen);
    ~StreamingManager();

    /**
     * @brief 异步请求加载指定 trunk
     * @param trunkId trunk 唯一标识
     * @param onLoaded 加载完成后（主线程 flush 时）的可选回调
     */
    void requestLoad(const std::string& trunkId, std::function<void()> onLoaded = nullptr);

    /**
     * @brief 异步请求卸载指定 trunk
     * @param trunkId trunk 唯一标识
     */
    void requestUnload(const std::string& trunkId);

    /**
     * @brief 主线程调用，将后台已完成的任务提交到 DrawCommandGenerator
     */
    void flush();

    /**
     * @brief 停止后台线程
     */
    void shutdown();

private:
    struct Task {
        enum class Type { Load, Unload } type;
        std::string trunkId;
        std::function<void()> callback;
    };

    void workerLoop();
    void rebuildMdiCommands();

    GlobalSceneTable*    m_sceneTable;
    DrawCommandGenerator* m_commandGen;

    std::thread                m_workerThread;
    std::mutex                 m_queueMutex;
    std::condition_variable    m_cv;
    std::queue<Task>           m_pendingTasks;
    std::atomic<bool>          m_running{true};

    std::mutex                 m_completedMutex;
    std::vector<Task>          m_completedTasks;
};

} // namespace Core
} // namespace Nexus
