#ifndef PLAYERCORE_H
#define PLAYERCORE_H

#include <mutex>
#include <string>

#include "PacketQueue.h"
#include "FrameQueue.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>
}

class PlayerCore
{
public:
    PlayerCore()
    {
        video_f_que.pkt_que = &video_p_que;
        audio_f_que.pkt_que = &audio_p_que;
        subtitle_f_que.pkt_que = &subtitle_p_que;
    }

    ~PlayerCore()
    {

    }

    std::string filename;
    // AVInputFormat *iformat;
    AVFormatContext *fmt_ctx;

    enum class ShowMode {
        SHOW_MODE_NONE = -1,
        SHOW_MODE_VIDEO = 0,
        SHOW_MODE_AUDIO,
        SHOW_MODE_NB
    } show_mode;

    int video_stream;
    AVCodecParameters *video_codec_params;
    PacketQueue video_p_que;
    FrameQueue video_f_que;

    int audio_stream;
    AVCodecParameters *audio_codec_params;
    PacketQueue audio_p_que;
    FrameQueue audio_f_que;

    int subtitle_stream;
    AVCodecParameters *subtitle_codec_params;
    PacketQueue subtitle_p_que;
    FrameQueue subtitle_f_que;


    std::mutex m_mutex;     // 互斥锁
};

#endif // PLAYERCORE_H
