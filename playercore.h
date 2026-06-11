#ifndef PLAYERCORE_H
#define PLAYERCORE_H

#include <mutex>
#include <string>

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

    }

    ~PlayerCore()
    {

    }

    std::string filename;
    const AVInputFormat *iformat;
    AVFormatContext *ic;
    int video_stream;
    int audio_stream;
    int subtitle_stream;
    std::mutex m_mutex;     // 互斥锁
};

#endif // PLAYERCORE_H
