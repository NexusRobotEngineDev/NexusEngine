#pragma once
#include <vulkan/vulkan.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <functional>
#include "../Interfaces.h"

namespace Nexus {

class VK_Context;
class VK_Texture;

struct TextureUploadTask {
    VK_Texture* targetTexture;
    ImageData data;
};

class VK_TextureUploader {
public:
    VK_TextureUploader(VK_Context* context);
    ~VK_TextureUploader();

    void queueUpload(VK_Texture* targetTexture, const ImageData& data);

private:
    void threadLoop();

    VK_Context* m_context;
    vk::CommandPool m_commandPool;
    std::thread m_workerThread;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<TextureUploadTask> m_tasks;
    std::atomic<bool> m_running{true};
};

}
