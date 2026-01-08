#include "StreamingManager.h"
#include "Log.h"

namespace Nexus {
namespace Core {

StreamingManager::StreamingManager(GlobalSceneTable* sceneTable, DrawCommandGenerator* commandGen)
    : m_sceneTable(sceneTable), m_commandGen(commandGen) {
    m_workerThread = std::thread(&StreamingManager::workerLoop, this);
}

StreamingManager::~StreamingManager() {
    shutdown();
}

void StreamingManager::requestLoad(const std::string& trunkId, std::function<void()> onLoaded) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_pendingTasks.push({Task::Type::Load, trunkId, std::move(onLoaded)});
    m_cv.notify_one();
}

void StreamingManager::requestUnload(const std::string& trunkId) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_pendingTasks.push({Task::Type::Unload, trunkId, nullptr});
    m_cv.notify_one();
}

void StreamingManager::flush() {
    std::vector<Task> done;
    {
        std::lock_guard<std::mutex> lock(m_completedMutex);
        done.swap(m_completedTasks);
    }
    if (done.empty()) return;
    rebuildMdiCommands();
    for (auto& task : done) {
        if (task.callback) task.callback();
    }
}

void StreamingManager::shutdown() {
    m_running.store(false);
    m_cv.notify_all();
    if (m_workerThread.joinable()) m_workerThread.join();
}

void StreamingManager::workerLoop() {
    while (m_running.load()) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this] { return !m_pendingTasks.empty() || !m_running.load(); });
            if (!m_running.load() && m_pendingTasks.empty()) return;
            task = std::move(m_pendingTasks.front());
            m_pendingTasks.pop();
        }
        if (task.type == Task::Type::Load) {
            NX_CORE_INFO("StreamingManager: Loading trunk '{}'", task.trunkId);
            m_sceneTable->markLoaded(task.trunkId);
        } else {
            NX_CORE_INFO("StreamingManager: Unloading trunk '{}'", task.trunkId);
            m_sceneTable->markUnloaded(task.trunkId);
        }
        {
            std::lock_guard<std::mutex> lock(m_completedMutex);
            m_completedTasks.push_back(std::move(task));
        }
    }
}

void StreamingManager::rebuildMdiCommands() {
    auto entries = m_sceneTable->getLoadedEntries();
    std::vector<DrawIndexedIndirectCommand> commands;
    commands.reserve(entries.size());
    for (const auto& e : entries) {
        DrawIndexedIndirectCommand cmd{};
        cmd.indexCount    = e.indexCount;
        cmd.instanceCount = 1;
        cmd.firstIndex    = e.indexOffset;
        cmd.vertexOffset  = static_cast<int32_t>(e.vertexOffset);
        cmd.firstInstance = 0;
        commands.push_back(cmd);
    }
    (void)m_commandGen->updateCommands(commands);
}

} // namespace Core
} // namespace Nexus
