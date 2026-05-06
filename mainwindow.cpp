#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);


    // player = new QMediaPlayer;
    // vw = new QVideoWidget;

    // // 将player绑定到显示的窗口上
    // player->setVideoOutput(vw);

    // // 打开播放器的位置
    // player->setSource(QUrl::fromLocalFile("D:/Code/Audio and Video Development/shortVideo.mp4"));
    // // vw->setGeometry(100,100,640,480);
    // // vw->show();
    vw = new VideoWidget;
    ui->centralwidget->layout()->addWidget(vw);
    vw->setParent(this->centralWidget());

    // player->play();


    // vw->setParent(this);

    // QVBoxLayout *vBox = new QVBoxLayout(this->centralWidget());
    // this->centralWidget()->setLayout(vBox);
    // vBox->setContentsMargins(0, 0, 0, 0);   // 外边框距设为0
    // vBox->setSpacing(0);                    // 控件间距设为0
    // vBox->addWidget(vw);
    // QSize centralWidgetSize = this->centralWidget()->size();
    // vw->setBaseSize(centralWidgetSize);
    // // vw->setFixedSize(QSize(800,600));
}

MainWindow::~MainWindow()
{
    delete ui;
}
