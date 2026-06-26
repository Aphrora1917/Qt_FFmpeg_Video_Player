#ifndef DECODER_H
#define DECODER_H

#include "packetQueue.h"

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
    PacketQueue *queue;
    AVCodecContext *avctx;

    // 【必须保留的状态字段】
    AVPacket *pkt;          // 工作包（需常驻内存）
    int pkt_serial;         // 当前包序列号
    int finished;           // 结束序列号
    int packet_pending;     // 挂起重试标志

    // 【音频 PTS 预测】
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
};


#endif // DECODER_H
