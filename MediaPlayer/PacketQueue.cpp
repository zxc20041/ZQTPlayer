#include "PacketQueue.h"

PacketQueue::PacketQueue(size_t maxSize)
    : m_maxSize(maxSize) {}

PacketQueue::~PacketQueue()
{
    flush();
}

bool PacketQueue::push(AVPacket *pkt)
{
    std::unique_lock lock(m_mutex);
    // Block until there is room or we are told to abort
    m_condPush.wait(lock, [this] { return m_aborted || m_queue.size() < m_maxSize; });
    if (m_aborted) return false;

    AVPacket *clone = av_packet_clone(pkt);
    m_queue.push(clone);
    m_condPop.notify_one();          // wake one waiting consumer
    return true;
}

bool PacketQueue::pop(AVPacket **out)
{
    std::unique_lock lock(m_mutex);
    // Block until a packet is available or we are told to abort
    m_condPop.wait(lock, [this] { return m_aborted || !m_queue.empty(); });
    if (m_aborted && m_queue.empty()) return false;

    *out = m_queue.front();
    m_queue.pop();
    m_condPush.notify_one();         // wake one waiting producer
    return true;
}

void PacketQueue::flush()
{
    std::lock_guard lock(m_mutex);
    while (!m_queue.empty()) {
        AVPacket *pkt = m_queue.front();
        m_queue.pop();
        av_packet_free(&pkt);
    }
    // After flush the queue is empty — wake any blocked producer
    m_condPush.notify_all();
}

void PacketQueue::abort()
{
    std::lock_guard lock(m_mutex);
    m_aborted = true;
    m_condPush.notify_all();
    m_condPop.notify_all();
}

void PacketQueue::restart()
{
    std::lock_guard lock(m_mutex);
    m_aborted = false;
}

void PacketQueue::setMaxSize(size_t maxSize)
{
    std::lock_guard lock(m_mutex);
    m_maxSize = maxSize;
    // If capacity increased, wake producers that may have been blocked
    m_condPush.notify_all();
}

size_t PacketQueue::size() const
{
    std::lock_guard lock(m_mutex);
    return m_queue.size();
}

bool PacketQueue::empty() const
{
    std::lock_guard lock(m_mutex);
    return m_queue.empty();
}
