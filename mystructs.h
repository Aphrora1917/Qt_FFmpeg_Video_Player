#ifndef MYSTRUCTS_H
#define MYSTRUCTS_H

#include <atomic>
#include <cstddef>
#include <utility>
#include <string>
#include <iostream>
#include <memory>

extern "C"
{
#include <libavutil/frame.h>
}

struct MyException
{
    MyException(std::string str) : msg(str) {}
    std::string msg;
};

struct Frame {
    AVFrame* frame = nullptr;
    double pts = 0.0;
    double duration = 0.0;
    int serial = 0;

    Frame() = default;
    ~Frame() { if (frame) av_frame_free(&frame); }

    // 支持对AVFrame的引用拷贝，支持移动
    // 拷贝构造
    Frame(const Frame& other) {
        // 对AVFrame*初始化分配空间
        if (!this->frame) {
            this->frame = av_frame_alloc();
        }
        if (!this->frame) {
            // 处理内存失败（通常抛出异常或终止）
            throw MyException("av_frame_alloc failed!");
        }
        int ret = av_frame_ref(this->frame, other.frame);
        if (ret < 0) {
            av_frame_free(&this->frame);
            // 处理错误（抛出异常）
            throw MyException("av_frame_ref failed!");
        }
        this->pts = other.pts;
        this->duration = other.duration;
        this->serial = other.serial;
    }

    // 拷贝赋值
    Frame& operator=(const Frame& other) {
        if (this == &other) return *this;

        // 赋值拷贝前先检查和释放自己的AVFrame*
        if (this->frame) {
            av_frame_unref(this->frame);
        } else {
            // 对AVFrame*初始化分配空间
            this->frame = av_frame_alloc();
            if (!this->frame) {
                // 处理内存失败（通常抛出异常或终止）
                throw MyException("av_frame_alloc failed!");
            }
        }

        int ret = av_frame_ref(this->frame, other.frame);
        if (ret < 0) {
            throw MyException("av_frame_ref failed!");
        }

        this->pts = other.pts;
        this->duration = other.duration;
        this->serial = other.serial;
        return *this;
    }

    // 移动构造
    Frame(Frame&& other) noexcept {
        this->frame = other.frame;
        other.frame = nullptr;
        this->pts = other.pts;
        this->duration = other.duration;
        this->serial = other.serial;
    }

    // 移动赋值
    Frame& operator=(Frame&& other) noexcept {
        if (this != &other) {
            if (frame) av_frame_free(&frame);    // destruct old AVFrame
            this->frame = other.frame;
            other.frame = nullptr;
            this->pts = other.pts;
            this->duration = other.duration;
            this->serial = other.serial;
        }
        return *this;
    }
};


namespace spsc {

// 支持非 POD 类型的 SPSC 环形缓冲区
// T 必须支持移动构造（nothrow 移动构造最佳），允许拷贝构造
template <typename T, size_t PowerOfTwoSize>
class RingBuffer {
    static_assert((PowerOfTwoSize & (PowerOfTwoSize - 1)) == 0,
                  "PowerOfTwoSize must be a power of 2");
    static_assert(PowerOfTwoSize > 0, "Size must be > 0");

    static constexpr size_t kMask = PowerOfTwoSize - 1;
    static constexpr size_t kCapacity = PowerOfTwoSize;   // 总槽位数（含一个空位）
    static constexpr size_t sizeofT = sizeof(T);


    alignas(64) std::atomic<size_t> head_;   // 生产者下一次写入的位置
    alignas(64) std::atomic<size_t> tail_;   // 消费者下一次读取的位置

    // 声明原始字节块，避免提前构造对象
    alignas(alignof(T)) std::byte buffer_[kCapacity * sizeof(T)];

public:
    RingBuffer() : head_(0), tail_(0) {}

    // 禁止拷贝和移动（移动整个缓冲区不合理）
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // 析构函数：清理所有剩余元素
    ~RingBuffer() {
        clear();
    }

    // ---------- 生产者接口 ----------
    // 左值版本（拷贝构造，前提是 T 支持拷贝构造，否则禁用或删除）
    bool push(const T& item) {
        size_t cur_head = head_.load(std::memory_order_relaxed);
        size_t cur_tail = tail_.load(std::memory_order_acquire);
        if (((cur_head + 1) & kMask) == cur_tail) {
            return false;   // 满
        }
        // placement new 拷贝构造
        new (&buffer_[cur_head * sizeofT]) T(item);
        head_.store((cur_head + 1) & kMask, std::memory_order_release);
        return true;
    }

    // 右值版本（移动构造，适用于 Frame）
    bool push(T&& item) {
        size_t cur_head = head_.load(std::memory_order_relaxed);
        size_t cur_tail = tail_.load(std::memory_order_acquire);
        if (((cur_head + 1) & kMask) == cur_tail) {
            return false;
        }
        // placement new 移动构造
        new (&buffer_[cur_head * sizeofT]) T(std::move(item));
        head_.store((cur_head + 1) & kMask, std::memory_order_release);
        return true;
    }


    // ---------- 可覆盖旧数据的生产者接口 ----------
    // 左值版本：如果缓冲区满，cur_head插入新值后需要将head和tail都+1；否则正常插入
    void push_or_overwrite(const T& item) {
        size_t cur_head = head_.load(std::memory_order_relaxed);
        size_t cur_tail = tail_.load(std::memory_order_acquire);

        // 判断缓冲区是否已满
        bool is_full = ((cur_head + 1) & kMask) == cur_tail;

        if (!is_full) {
            // 不满：正常插入（与 push 逻辑相同）
            T* slot = reinterpret_cast<T*>(&buffer_[cur_head * sizeofT]);
            new (slot) T(item);
            head_.store((cur_head + 1) & kMask, std::memory_order_release);
        } else {
            T* slot = reinterpret_cast<T*>(&buffer_[cur_head * sizeofT]);
            new (slot) T(std::move(item));

            // 同时移动 head 和 tail（二者均向前一步）
            size_t new_head = (cur_head + 1) & kMask;
            size_t new_tail = (cur_tail + 1) & kMask;

            // 使用 release 语义发布更新，确保新数据对消费者可见
            head_.store(new_head, std::memory_order_release);
            tail_.store(new_tail, std::memory_order_release);
        }
    }

    // 右值版本（移动语义）
    void push_or_overwrite(T&& item) {
        size_t cur_head = head_.load(std::memory_order_relaxed);
        size_t cur_tail = tail_.load(std::memory_order_acquire);

        bool is_full = ((cur_head + 1) & kMask) == cur_tail;

        if (!is_full) {
            T* slot = reinterpret_cast<T*>(&buffer_[cur_head * sizeofT]);
            new (slot) T(std::move(item));
            head_.store((cur_head + 1) & kMask, std::memory_order_release);
        } else {
            T* slot = reinterpret_cast<T*>(&buffer_[cur_head * sizeofT]);
            new (slot) T(std::move(item));

            size_t new_head = (cur_head + 1) & kMask;
            size_t new_tail = (cur_tail + 1) & kMask;

            head_.store(new_head, std::memory_order_release);
            tail_.store(new_tail, std::memory_order_release);
        }
    }

    // ---------- 消费者接口 ----------
    // 读取并移除一个元素
    bool pop(T& out) {
        size_t cur_tail = tail_.load(std::memory_order_relaxed);
        size_t cur_head = head_.load(std::memory_order_acquire);
        if (cur_tail == cur_head) {
            return false;   // 空
        }
        // 移动数据到外部
        out = std::move(*reinterpret_cast<T*>(&buffer_[cur_tail * sizeofT]));
        // 显式析构缓冲区中的对象
        reinterpret_cast<T*>(&buffer_[cur_tail * sizeofT])->~T();
        tail_.store((cur_tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    // 跳过（移除）一个元素，不读取
    bool skip() {
        size_t cur_tail = tail_.load(std::memory_order_relaxed);
        size_t cur_head = head_.load(std::memory_order_acquire);
        if (cur_tail == cur_head) {
            return false;
        }
        reinterpret_cast<T*>(&buffer_[cur_tail * sizeofT])->~T();
        tail_.store((cur_tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    // 批量跳过多个元素，返回实际跳过的个数
    size_t skip(size_t count) {
        size_t cur_tail = tail_.load(std::memory_order_relaxed);
        size_t cur_head = head_.load(std::memory_order_acquire);
        size_t avail = (cur_head - cur_tail) & kMask;   // 可读元素数
        if (count > avail) count = avail;
        for (size_t i = 0; i < count; ++i) {
            reinterpret_cast<T*>(&buffer_[((cur_tail + i) & kMask) * sizeofT])->~T();
        }
        tail_.store((cur_tail + count) & kMask, std::memory_order_release);
        return count;
    }

    // 查看队首元素（只读）
    const T* peek() const {
        size_t cur_tail = tail_.load(std::memory_order_relaxed);
        size_t cur_head = head_.load(std::memory_order_acquire);
        if (cur_tail == cur_head) return nullptr;
        return reinterpret_cast<const T*>(&buffer_[cur_tail * sizeofT]);
    }

    // 查看第 n 个元素（0 表示下一个）
    const T* at(size_t index) const {
        size_t cur_tail = tail_.load(std::memory_order_relaxed);
        size_t cur_head = head_.load(std::memory_order_acquire);
        size_t avail = (cur_head - cur_tail) & kMask;
        if (index >= avail) return nullptr;
        return reinterpret_cast<const T*>(&buffer_[((cur_tail + index) & kMask) * sizeofT]);
    }

    // 状态查询
    bool isEmpty() const {
        size_t cur_tail = tail_.load(std::memory_order_relaxed);
        size_t cur_head = head_.load(std::memory_order_acquire);
        return cur_tail == cur_head;
    }

    bool isFull() const {
        size_t cur_head = head_.load(std::memory_order_relaxed);
        size_t cur_tail = tail_.load(std::memory_order_acquire);
        return ((cur_head + 1) & kMask) == cur_tail;
    }

    size_t count() const {
        size_t cur_tail = tail_.load(std::memory_order_relaxed);
        size_t cur_head = head_.load(std::memory_order_acquire);
        return (cur_head - cur_tail) & kMask;
    }

    static constexpr size_t capacity() {
        return kCapacity - 1;   // 实际可用元素个数
    }

    // 清空缓冲区（仅消费者调用，或确保无并发读写时调用）
    void clear() {
        size_t cur_tail = tail_.load(std::memory_order_relaxed);
        size_t cur_head = head_.load(std::memory_order_acquire);
        size_t cnt = (cur_head - cur_tail) & kMask;
        for (size_t i = 0; i < cnt; ++i) {
            reinterpret_cast<T*>(&buffer_[((cur_tail + i) & kMask) * sizeofT])->~T();
        }
        tail_.store(cur_head, std::memory_order_release);
    }

    // 仅供消费者debug，读取并打印输出ringbuffer状态和各个元素
    void debug_output(void (*func)(std::shared_ptr<T[]>, int)) {
        size_t cur_tail = tail_.load(std::memory_order_relaxed);
        size_t cur_head = head_.load(std::memory_order_acquire);
        bool isFull = ((cur_head + 1) & kMask) == cur_tail;
        bool isEmpty = cur_tail == cur_head;
        size_t avail = (cur_head - cur_tail) & kMask;
        std::cout << "======================================" << std::endl;
        std::cout << "cur_head: " << cur_head << std::endl;
        std::cout << "cur_tail: " << cur_tail << std::endl;
        std::cout << "isFull: " << isFull << std::endl;
        std::cout << "isEmpty: " << isEmpty << std::endl;
        std::cout << "avail: " << avail << std::endl;
        if (!isEmpty) {
            std::shared_ptr<T[]> buff(new T[avail]);
            for (int i = 0; i < avail; ++i) {
                buff[i] = *reinterpret_cast<T*>(&buffer_[((cur_tail + i) & kMask) * sizeofT]);
            }
            func(buff, avail);
        }
    }
};

} // namespace spsc

#endif // MYSTRUCTS_H
