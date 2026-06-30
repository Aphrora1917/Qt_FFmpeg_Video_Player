#ifndef DECODER_H
#define DECODER_H

#include "packetQueue.h"
#include <condition_variable>
#include <memory>
#include <thread>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class Decoder
{
public:
    Decoder()
    {

    }

    PacketQueue *queue = nullptr;         // 【传送带】指向对应的 PacketQueue（视频/音频/字幕）
    AVCodecContext *avctx = nullptr;      // 【引擎】解码器上下文（真正的解码核心）

    AVPacket *pkt = nullptr;              // 【工作台】当前正在处理的 AVPacket 指针（常驻内存）
    int pkt_serial = 0;             // 【序列号标记】当前正在处理的包属于第几代（用于 Seek 检测）
    int finished = 0;               // 【结束标记】记录解码器结束时的序列号（用于 EOF 判断）
    int packet_pending = 0;         // 【挂起重试标志】若当前 pkt 还没成功 send 给解码器，下次重试

    int64_t start_pts = 0;          // 【音频锚点】针对无时间戳格式，记录音频起始 PTS
    AVRational start_pts_tb{0, 0};    // 【音频锚点时间基】start_pts 的时间基
    int64_t next_pts = 0;           // 【音频预测指针】手动推算的下一帧音频 PTS
    AVRational next_pts_tb{0, 0};     // 【音频预测时间基】next_pts 的时间基

    // 解码线程
    // 解码线程不需要返回值，线程生命周期由 join() 函数确保，解码线程的 join() 在 decoder_abort() 中执行
    std::thread decoder_thread;
    std::shared_ptr<std::condition_variable> empty_queue_cond = nullptr;  // 【反向通知喇叭】PacketQueue队列空时，用来叫醒 read_thread 赶紧读数据
};


#endif // DECODER_H
