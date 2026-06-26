#ifndef FRAMEQUEUE_H
#define FRAMEQUEUE_H

#include <string>

#include "packetQueue.h"
#include "ringbuffer.h"

struct MyException
{
    MyException(std::string str) : msg(str) {}
    std::string msg;
};

extern "C"
{
#include <libavutil/frame.h>
}

struct Frame {
    AVFrame* frame = nullptr;   // 音视频数据载体（必须）
    double pts = 0.0;           // 展示时间戳，用于音视频同步（必须）
    double duration = 0.0;      // 帧持续时间，用于计算渲染延迟（必须）
    int serial = 0;             // 序列号，用于Seek后丢弃旧帧（必须）

    Frame() = default;
    ~Frame() { if (frame) av_frame_free(&frame); }

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

    // 显式禁止拷贝构造和拷贝赋值
    Frame(const Frame& other) = delete;
    Frame& operator=(const Frame& other) = delete;

    // // 拷贝构造
    // Frame(const Frame& other) {
    //     // 对AVFrame*初始化分配空间
    //     if (!this->frame) {
    //         this->frame = av_frame_alloc();
    //     }
    //     if (!this->frame) {
    //         // 处理内存失败（通常抛出异常或终止）
    //         throw MyException("av_frame_alloc failed!");
    //     }
    //     int ret = av_frame_ref(this->frame, other.frame);
    //     if (ret < 0) {
    //         av_frame_free(&this->frame);
    //         // 处理错误（抛出异常）
    //         throw MyException("av_frame_ref failed!");
    //     }
    //     this->pts = other.pts;
    //     this->duration = other.duration;
    //     this->serial = other.serial;
    // }

    // // 拷贝赋值
    // Frame& operator=(const Frame& other) {
    //     if (this == &other) return *this;

    //     // 赋值拷贝前先检查和释放自己的AVFrame*
    //     if (this->frame) {
    //         av_frame_unref(this->frame);
    //     } else {
    //         // 对AVFrame*初始化分配空间
    //         this->frame = av_frame_alloc();
    //         if (!this->frame) {
    //             // 处理内存失败（通常抛出异常或终止）
    //             throw MyException("av_frame_alloc failed!");
    //         }
    //     }

    //     int ret = av_frame_ref(this->frame, other.frame);
    //     if (ret < 0) {
    //         throw MyException("av_frame_ref failed!");
    //     }

    //     this->pts = other.pts;
    //     this->duration = other.duration;
    //     this->serial = other.serial;
    //     return *this;
    // }
};

class FrameQueue : public RingBuffer<Frame, 8>
{
public:
    FrameQueue() : RingBuffer<Frame, 8>() {};

    PacketQueue *pkt_que = nullptr;
};

#endif // FRAMEQUEUE_H
