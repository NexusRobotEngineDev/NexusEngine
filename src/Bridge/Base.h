#pragma once

#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <iostream>
#include <cassert>
#include <string>
#include <utility>
#include <cstddef>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>

/**
 * @brief 核心基础定义与工具
 */

using Status = absl::Status;

template <typename T>
using StatusOr = absl::StatusOr<T>;

using absl::OkStatus;
using absl::NotFoundError;
using absl::InvalidArgumentError;
using absl::InternalError;
using absl::AbortedError;

namespace details {
template <typename T>
inline Status getStatus(const StatusOr<T>& s) { return s.status(); }
inline Status getStatus(const Status& s) { return s; }
}

#define NX_CONCAT_IMPL(x, y) x##y
#define NX_CONCAT(x, y) NX_CONCAT_IMPL(x, y)

/**
 * @brief 检查状态,如果非 OK 则直接返回该状态
 * 支持 Status 和 StatusOr
 */
#define NX_RETURN_IF_ERROR(expr) \
    do { \
        auto NX_CONCAT(_status_, __LINE__) = (expr); \
        if (!NX_CONCAT(_status_, __LINE__).ok()) return ::details::getStatus(NX_CONCAT(_status_, __LINE__)); \
    } while (0)

/**
 * @brief 检查 StatusOr, 如果 OK 则解构到变量, 否则返回状态
 */
#define NX_ASSIGN_OR_RETURN(lhs, expr) \
    auto NX_CONCAT(_res_or_, __LINE__) = (expr); \
    if (!NX_CONCAT(_res_or_, __LINE__).ok()) return NX_CONCAT(_res_or_, __LINE__).status(); \
    lhs = std::move(NX_CONCAT(_res_or_, __LINE__)).value()

/**
 * @brief 自定义断言宏
 */
#ifdef NDEBUG
#define NX_ASSERT(expr, msg) ((void)0)
#else
#define NX_ASSERT(expr, msg) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: (" << #expr << "), message: " << msg \
                      << ", file: " << __FILE__ << ", line: " << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)
#endif

namespace Nexus {

/**
 * @brief 简单的多线程基类
 */
class Thread {
public:
    Thread() : m_running(false) {}
    virtual ~Thread() { stop(); }

    void start(std::function<void()> func) {
        m_running = true;
        m_thread = std::thread([this, func]() {
            func();
            m_running = false;
        });
    }

    void stop() {
        m_running = false;
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    bool isRunning() const { return m_running; }

protected:
    std::atomic<bool> m_running;

private:
    std::thread m_thread;
};

/**
 * @brief 单生产者单消费者队列
 */
template<typename T, size_t Size>
class SPSCQueue {
public:
    bool push(const T& item) {
        size_t next_head = (m_head.load(std::memory_order_relaxed) + 1) % Size;
        if (next_head == m_tail.load(std::memory_order_acquire)) return false;
        m_data[m_head.load(std::memory_order_relaxed)] = item;
        m_head.store(next_head, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        if (m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_relaxed)) return false;
        item = m_data[m_tail.load(std::memory_order_relaxed)];
        m_tail.store((m_tail.load(std::memory_order_relaxed) + 1) % Size, std::memory_order_release);
        return true;
    }

private:
    T m_data[Size];
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};
};

} // namespace Nexus
