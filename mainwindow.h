#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QDebug>
#include <QLayout>
#include <QResizeEvent>
#include "videowidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // // 创建一个MediaPlayer
    // QMediaPlayer *player;

    // // 创建一个播放器窗口（Widget），用于显示MediaPlayer
    // QVideoWidget *vw;

    VideoWidget *vw;

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
