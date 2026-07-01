#ifndef PLAYERCORE_H
#define PLAYERCORE_H

#include <future>
#include <string>
#include <memory>

#include "packetQueue.h"
#include "frameQueue.h"
#include "decoder.h"
#include "clock.h"

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
        continue_read_thread = std::make_shared<std::condition_variable>();
    }

    ~PlayerCore()
    {

    }

    std::string filename;
    // AVInputFormat *iformat;
    AVFormatContext *fmt_ctx;
    int realtime;

    int width, height, xleft, ytop;
    int step;

    enum class ShowMode {
        SHOW_MODE_NONE = -1,
        SHOW_MODE_VIDEO = 0,
        SHOW_MODE_AUDIO,
        SHOW_MODE_NB
    } show_mode;

    int abort_request;
    int paused;
    int last_paused;
    int queue_attachments_req;
    int seek_req;                   // 标识一次SEEK请求
    int seek_flags;                 // SEEK标志，诸如AVSEEK_FLAG_BYTE等
    int64_t seek_pos;               // SEEK的目标位置(当前位置+增量)
    int64_t seek_rel;               // 本次SEEK的位置增量
    int read_pause_return;
    int loop = 1;
    int64_t start_time = AV_NOPTS_VALUE;
    bool autoexit = true;


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

    Clock audclk;                   // 音频时钟
    Clock vidclk;                   // 视频时钟
    Clock extclk;                   // 外部时钟

    double frame_timer;             // 记录最后一帧播放的时刻(单位是秒)
    double max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity

    int last_audio_stream;
    int last_subtitle_stream;
    int last_video_stream;

    int eof = 0;

    // read_thread 解封装线程需要返回值，采用 async 启动，由 future 获得返回值，async 返回的 future 移动给成员变量管理线程生命周期
    std::unique_ptr<std::future<int>> future_read;

    // std::mutex m_mutex;     // 互斥锁
    std::shared_ptr<std::condition_variable> continue_read_thread;   // 为控制read_thread的条件变量，它关联的锁在read_thread_func()内部定义
};

#endif // PLAYERCORE_H
