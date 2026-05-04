#include "bridge/core_bridge.hpp"
#include <QMetaObject>

namespace AviQtl::UI {

CoreBridge &CoreBridge::instance() {
    static CoreBridge inst;
    return inst;
}

void CoreBridge::requestSeek(int frame) {
    int tail = m_tail.load(std::memory_order_relaxed);
    int next = (tail + 1) % QUEUE_CAPACITY;
    if (next == m_head.load(std::memory_order_acquire))
        return;
    m_queue[tail] = {CommandType::Seek, frame};
    m_tail.store(next, std::memory_order_release);
}

void CoreBridge::requestPlay() {
    int tail = m_tail.load(std::memory_order_relaxed);
    int next = (tail + 1) % QUEUE_CAPACITY;
    if (next == m_head.load(std::memory_order_acquire))
        return;
    m_queue[tail] = {CommandType::Play, 0};
    m_tail.store(next, std::memory_order_release);
}

void CoreBridge::requestPause() {
    int tail = m_tail.load(std::memory_order_relaxed);
    int next = (tail + 1) % QUEUE_CAPACITY;
    if (next == m_head.load(std::memory_order_acquire))
        return;
    m_queue[tail] = {CommandType::Pause, 0};
    m_tail.store(next, std::memory_order_release);
}

bool CoreBridge::dequeueCommand(Command &out) {
    int head = m_head.load(std::memory_order_relaxed);
    if (head == m_tail.load(std::memory_order_acquire))
        return false;
    out = m_queue[head];
    m_head.store((head + 1) % QUEUE_CAPACITY, std::memory_order_release);
    return true;
}

void CoreBridge::notifyFrameAdvanced(int frame) {
    m_currentFrame.store(frame, std::memory_order_relaxed);
    QMetaObject::invokeMethod(this, [this, frame]() { emit currentFrameChanged(frame); }, Qt::QueuedConnection);
}

} // namespace AviQtl::UI
