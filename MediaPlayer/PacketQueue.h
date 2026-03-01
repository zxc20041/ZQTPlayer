#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

extern "C" {
#include <libavcodec/avcodec.h>
}

/// @brief Thread-safe bounded blocking queue for AVPacket pointers.
///
/// Designed for the producer-consumer pattern between the demux thread
/// (pushes packets) and the decode thread (pops packets).
class PacketQueue
{
public:
    explicit PacketQueue(size_t maxSize = 128);
    ~PacketQueue();

    // Non-copyable, non-movable
    PacketQueue(const PacketQueue &) = delete;
    PacketQueue &operator=(const PacketQueue &) = delete;

    /// Enqueue a packet (blocks if full, returns false if aborted).
    /// The queue takes ownership of a clone — caller still owns @p pkt.
    bool push(AVPacket *pkt);

    /// Dequeue a packet (blocks if empty, returns false if aborted).
    /// On success *out is a newly allocated AVPacket; caller must av_packet_free().
    bool pop(AVPacket **out);

    /// Drop all buffered packets, freeing their memory.
    void flush();

    /// Wake all blocked threads so they can exit.
    void abort();

    /// Reset the aborted flag so the queue can be reused after a seek/stop.
    void restart();

    /// Change the maximum capacity. Takes effect immediately.
    void setMaxSize(size_t maxSize);

    size_t size() const;
    bool   empty() const;

private:
    mutable std::mutex        m_mutex;
    std::condition_variable   m_condPush;   // producer waits here when queue is full
    std::condition_variable   m_condPop;    // consumer waits here when queue is empty
    std::queue<AVPacket *>    m_queue;
    size_t                    m_maxSize;
    bool                      m_aborted = false;
};
