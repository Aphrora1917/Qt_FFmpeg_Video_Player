#ifndef PLAYERCORE_H
#define PLAYERCORE_H

#include <mutex>
#include <string>

#include "packetQueue.h"
#include "frameQueue.h"
#include "decoder.h"

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
    AVStream *video_st;
    AVCodecParameters *video_codec_params;
    PacketQueue video_p_que;
    FrameQueue video_f_que;

    int audio_stream;
    AVStream *audio_st;
    AVCodecParameters *audio_codec_params;
    PacketQueue audio_p_que;
    FrameQueue audio_f_que;

    int subtitle_stream;
    AVStream *subtitle_st;
    AVCodecParameters *subtitle_codec_params;
    PacketQueue subtitle_p_que;
    FrameQueue subtitle_f_que;

    Decoder auddec;
    Decoder viddec;
    Decoder subdec;

    int last_audio_stream;
    int last_subtitle_stream;
    int last_video_stream;

    int eof = 0;

    std::mutex m_mutex;     // 互斥锁
};

#endif // PLAYERCORE_H
