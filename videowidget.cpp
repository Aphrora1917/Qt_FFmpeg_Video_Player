#include "videowidget.h"
#include "ui_videowidget.h"

VideoWidget::VideoWidget(QWidget *parent)
    : QFrame(parent)
    , ui(new Ui::VideoWidget)
{
    ui->setupUi(this);
    createRightPopActions();

    player = new QMediaPlayer;

    // av_widget = new QVideoWidget(this);
    // ui->horizontalLayout->addWidget(av_widget);
    player->setVideoOutput(ui->av_widget); //把player绑定到显示窗口上
    // av_widget->show();

    ui->lineEdit->setText("http://192.168.5.1:81/stream");

    this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    ui->av_widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    // this->setStyleSheet(R"(QWidget{
    //                         background-color: rgb(0, 210, 210);
    //                         }
    //                         )");
    this->setLineWidth(4);

    qDebug() << avcodec_version();
    qDebug() << avcodec_license();
}

/* 右键弹出 Menu & Action */
void VideoWidget::createRightPopActions()
{
    popMenu = new QMenu(this);
    popMenu->setStyleSheet("background-color: rgb(100, 100, 100);");

    openLocalAction = new QAction(this);
    openLocalAction->setText(QString("打开本地文件"));

    openUrlAction = new QAction(this);
    openUrlAction->setText(QString("打开网络文件"));

    fullScreenAction = new QAction(this);
    fullScreenAction->setText(QString("全屏"));

    normalScreenAction = new QAction(this);
    normalScreenAction->setText(QString("普通"));


    quitAction = new QAction(this);
    quitAction->setText(QString("退出"));

    connect(openLocalAction, SIGNAL(triggered(bool)), this, SLOT(openLocalVideoSlot()));
    connect(openUrlAction, SIGNAL(triggered(bool)), this, SLOT(openUrlVideoSlot()));
    connect(fullScreenAction, &QAction::triggered, this, &VideoWidget::showFullScreen);
    connect(normalScreenAction, &QAction::triggered, this, &VideoWidget::showNormal);
    connect(quitAction, &QAction::triggered, this, &VideoWidget::close);
}

void VideoWidget::openLocalVideoSlot()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Open file"),
                                                QDir::homePath(),
                                                tr("Multimedia files(*)"));

    if (file.isEmpty())
        return;

    player->setSource(QUrl::fromLocalFile(file)); //设置需要打开的媒体文件

    player->play();
}

void VideoWidget::openUrlVideoSlot()
{
    QString url_str = ui->lineEdit->text();
    player->setSource(QUrl(url_str)); //设置需要打开的媒体文件
    qDebug() << url_str;
    player->play();
}



/*右键菜单接口*/
void VideoWidget::contextMenuEvent(QContextMenuEvent *event)
{
    Q_UNUSED(event);         //未使用不警告
    popMenu->clear();


    popMenu->addAction(openLocalAction);
    popMenu->addAction(openUrlAction);
    popMenu->addAction(fullScreenAction);
    popMenu->addAction(normalScreenAction);
    popMenu->addAction(quitAction);

    popMenu->exec(QCursor::pos());//QAction *exec(const QPoint &pos, QAction *at = Q_NULLPTR);
}

VideoWidget::~VideoWidget()
{
    delete ui;
}
