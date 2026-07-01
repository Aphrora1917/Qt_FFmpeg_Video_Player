#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QFrame>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QMenu>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QMouseEvent>

#include <string>
#include <iostream>
#include <chrono>
#include <cstdio>
#include <thread>
#include <future>

#include "playercore.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>
#include "libavutil/time.h"
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

void output_ffmeg_error(int ret)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, sizeof(errbuf));
    std::cout << "Error code: " << ret
              << ", message: " << errbuf << std::endl;
}

namespace Ui {
class VideoWidget;
}

class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

    int openVideo_single_thread(const std::string &_url);
    std::shared_ptr<PlayerCore> open_video(const std::string &_url);
    void renderingFrame(const AVFrame* _frame);
    void play();
    void pause();
    void end();
    void setPlaybackRate(float _rate);
    void jumpToTime(double _time);
    void setImage(const QImage& image);

public: signals:
    void updateImg(const QImage &_img);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseEvent(QMouseEvent *event);

private:
    int read_thread_func(std::shared_ptr<PlayerCore> _pc);
    int is_realtime(AVFormatContext *s);
    int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);
    void stream_seek(std::shared_ptr<PlayerCore> _pc, int64_t pos, int64_t rel, int by_bytes);
    void set_clock(Clock *c, double pts, int serial);
    void set_clock_at(Clock *c, double pts, int serial, double time);
    double get_clock(Clock *c);
    void step_to_next_frame(std::shared_ptr<PlayerCore> _pc);
    void stream_toggle_pause(std::shared_ptr<PlayerCore> _pc);
    int stream_component_open(std::shared_ptr<PlayerCore> _pc, int stream_index);
    int decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, std::shared_ptr<std::condition_variable> empty_queue_cond);
    int decoder_start(Decoder *d, std::function<int(std::shared_ptr<PlayerCore>)> fn, std::shared_ptr<PlayerCore> _pc);
    void packet_queue_start(PacketQueue *queue);
    void decoder_abort(Decoder *d, FrameQueue *fq);

    int video_thread_func(std::shared_ptr<PlayerCore> _pc);
    int audio_thread_func(std::shared_ptr<PlayerCore> _pc);
    int subtitle_thread_func(std::shared_ptr<PlayerCore> _pc);

    void write_yuv_to_file(AVFrame* frame);

    static int decode_interrupt_cb(void *ctx)
    {
        return ((PlayerCore *)ctx)->abort_request;
    }
private:
    Ui::VideoWidget *ui;

    std::shared_ptr<PlayerCore> m_pc;

    std::string url;
    bool isPlay;
    QImage currentImage;
};

#endif // VIDEOWIDGET_H
