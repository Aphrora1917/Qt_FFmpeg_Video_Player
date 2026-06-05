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
#include <cstdio>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
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

    int openVideo(const std::string &_url);
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
    void write_yuv_to_file(AVFrame* frame);

private:
    Ui::VideoWidget *ui;

    std::string url;
    bool isPlay;
    QImage currentImage;
};

#endif // VIDEOWIDGET_H
