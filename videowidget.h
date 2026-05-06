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

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

namespace Ui {
class VideoWidget;
}

class VideoWidget : public QFrame
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

protected:
    virtual void contextMenuEvent(QContextMenuEvent *event);

private:
    void createRightPopActions();

private slots:
    void openLocalVideoSlot();
    void openUrlVideoSlot();

private:
    Ui::VideoWidget *ui;

    QMenu *popMenu;               //右键弹出式菜单
    QAction *openLocalAction;     //打开本地文件
    QAction *openUrlAction;       //打开网络文件
    QAction *fullScreenAction;    //全屏显示
    QAction *normalScreenAction;  //普通显示
    QAction *quitAction;          //退出

    QMediaPlayer *player;


    // QVideoWidget* av_widget;    //不需要了,界面上已经拖了一个QVideoWidget进去了
};

#endif // VIDEOWIDGET_H
